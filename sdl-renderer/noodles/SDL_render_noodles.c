/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty. See SDL's zlib license for the complete terms.

  MiSTer-Noodles renderer. Protocol 1.3 handles opaque fills, unscaled copies,
  modulation, and blending. SDL's software routines provide coherent
  fallbacks for the remaining drawing operations.
*/
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_RENDER_NOODLES

#include "SDL_render_noodles.h"

#include "SDL_error.h"
#include "SDL_hints.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include "../software/SDL_blendfillrect.h"
#include "../software/SDL_blendline.h"
#include "../software/SDL_blendpoint.h"
#include "../software/SDL_drawline.h"
#include "../software/SDL_drawpoint.h"
#include "../software/SDL_triangle.h"

#include <errno.h>
#include <string.h>

#include <noodles_link.h>
#include <noodles_surface.h>

#define NOODLES_NATIVE_FORMAT SDL_PIXELFORMAT_ABGR8888
#define NOODLES_REQUIRED_PROTOCOL 0x00010003u
#define NOODLES_DEFAULT_RESIDENT_MB 192u
#define NOODLES_MAX_RESIDENT_MB 220u

typedef struct NOODLES_TextureData
{
    noodles_surface_t *surface;
    SDL_Surface *shadow;
    SDL_bool cpu_valid;
    SDL_bool gpu_valid;
    SDL_Rect lock_rect;
    SDL_bool locked;
    SDL_bool pinned;
    size_t resident_bytes;
    Uint64 last_use;
    struct NOODLES_TextureData *next;
} NOODLES_TextureData;

typedef struct NOODLES_RenderData
{
    noodles_link_t *link;
    NOODLES_TextureData composition;
    NOODLES_TextureData *target;
    NOODLES_TextureData *surfaces;
    size_t resident_bytes;
    size_t resident_budget;
    Uint64 use_clock;
    SDL_bool stats_enabled;
    Uint64 stats_start;
    Uint64 stats_frames;
    Uint64 stats_queue_ticks;
    Uint64 stats_queue_max_ticks;
    Uint64 stats_present_ticks;
    Uint64 stats_present_max_ticks;
} NOODLES_RenderData;

typedef struct NOODLES_CopyExData
{
    SDL_Rect srcrect;
    SDL_Rect dstrect;
    double angle;
    SDL_RendererFlip flip;
    float scale_x;
    float scale_y;
} NOODLES_CopyExData;

typedef struct NOODLES_GeometryFillData
{
    SDL_Point destination;
    SDL_Color color;
} NOODLES_GeometryFillData;

typedef struct NOODLES_GeometryCopyData
{
    SDL_Point source;
    SDL_Point destination;
    SDL_Color color;
} NOODLES_GeometryCopyData;

static int NOODLES_SetErrno(const char *operation)
{
    const int saved_errno = errno;
    return SDL_SetError("Noodles %s failed: %s", operation, strerror(saved_errno));
}

static Uint32 NOODLES_PackRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return (Uint32)r | ((Uint32)g << 8) | ((Uint32)b << 16) | ((Uint32)a << 24);
}

static int NOODLES_DrainLink(NOODLES_RenderData *data)
{
    if (noodles_link_drain(data->link, NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
        return NOODLES_SetErrno("drain");
    }
    return 0;
}

static size_t NOODLES_SurfaceBytes(int width, int height)
{
    const size_t pitch = ((size_t)width * 4u + 63u) & ~(size_t)63u;
    return (pitch * (size_t)height + 4095u) & ~(size_t)4095u;
}

static void NOODLES_TouchSurface(NOODLES_RenderData *data,
                                 NOODLES_TextureData *surface)
{
    surface->last_use = ++data->use_clock;
}

static int NOODLES_InitSurface(NOODLES_RenderData *data, int width, int height,
                               SDL_bool pinned, NOODLES_TextureData *surface)
{
    SDL_zerop(surface);
    surface->shadow = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                     NOODLES_NATIVE_FORMAT);
    if (!surface->shadow) {
        return -1;
    }
    SDL_memset(surface->shadow->pixels, 0,
               (size_t)surface->shadow->pitch * (size_t)surface->shadow->h);
    surface->cpu_valid = SDL_TRUE;
    surface->pinned = pinned;
    surface->resident_bytes = NOODLES_SurfaceBytes(width, height);
    surface->next = data->surfaces;
    data->surfaces = surface;
    return 0;
}

static void NOODLES_FreeSurface(NOODLES_RenderData *data,
                                NOODLES_TextureData *surface)
{
    NOODLES_TextureData **cursor = &data->surfaces;
    while (*cursor && *cursor != surface) {
        cursor = &(*cursor)->next;
    }
    if (*cursor) {
        *cursor = surface->next;
    }
    if (surface->surface) {
        (void)noodles_surface_destroy(surface->surface);
        data->resident_bytes -= surface->resident_bytes;
    }
    SDL_FreeSurface(surface->shadow);
    SDL_zerop(surface);
}

static int NOODLES_EnsureCPU(NOODLES_RenderData *data, NOODLES_TextureData *surface)
{
    noodles_rect_t rect;
    if (surface->cpu_valid) {
        return 0;
    }
    if (!surface->gpu_valid) {
        SDL_memset(surface->shadow->pixels, 0,
                   (size_t)surface->shadow->pitch * (size_t)surface->shadow->h);
        surface->cpu_valid = SDL_TRUE;
        return 0;
    }
    if (!surface->surface) {
        return SDL_SetError("Noodles evicted surface has no CPU contents");
    }
    rect.x = 0;
    rect.y = 0;
    rect.width = (Uint32)surface->shadow->w;
    rect.height = (Uint32)surface->shadow->h;
    if (noodles_surface_read(surface->surface, &rect, surface->shadow->pixels,
                             (size_t)surface->shadow->pitch,
                             NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
        return NOODLES_SetErrno("fallback readback");
    }
    surface->cpu_valid = SDL_TRUE;
    NOODLES_TouchSurface(data, surface);
    return 0;
}

static NOODLES_TextureData *NOODLES_FindEvictionCandidate(
    NOODLES_RenderData *data, const NOODLES_TextureData *exclude)
{
    NOODLES_TextureData *candidate = NULL;
    NOODLES_TextureData *surface;
    for (surface = data->surfaces; surface; surface = surface->next) {
        if (!surface->surface || surface->pinned || surface == exclude ||
            surface == data->target) {
            continue;
        }
        if (!candidate || surface->last_use < candidate->last_use) {
            candidate = surface;
        }
    }
    return candidate;
}

static int NOODLES_EvictSurface(NOODLES_RenderData *data,
                                NOODLES_TextureData *surface)
{
    size_t collected;
    if (NOODLES_EnsureCPU(data, surface) < 0) {
        return -1;
    }
    if (NOODLES_DrainLink(data) < 0) {
        return -1;
    }
    if (noodles_surface_destroy(surface->surface) < 0) {
        return NOODLES_SetErrno("surface eviction");
    }
    surface->surface = NULL;
    surface->gpu_valid = SDL_FALSE;
    data->resident_bytes -= surface->resident_bytes;
    if (noodles_surface_collect(data->link, &collected) < 0) {
        return NOODLES_SetErrno("surface collection");
    }
    return 0;
}

static int NOODLES_MakeResident(NOODLES_RenderData *data,
                                NOODLES_TextureData *surface)
{
    NOODLES_TextureData *candidate;
    SDL_bool collected_after_drain = SDL_FALSE;
    if (surface->surface) {
        NOODLES_TouchSurface(data, surface);
        return 0;
    }

    while (data->resident_bytes + surface->resident_bytes > data->resident_budget) {
        candidate = NOODLES_FindEvictionCandidate(data, surface);
        if (!candidate) {
            break;
        }
        if (NOODLES_EvictSurface(data, candidate) < 0) {
            return -1;
        }
    }

    for (;;) {
        if (noodles_surface_create(data->link, (Uint32)surface->shadow->w,
                                   (Uint32)surface->shadow->h,
                                   &surface->surface) == 0) {
            data->resident_bytes += surface->resident_bytes;
            NOODLES_TouchSurface(data, surface);
            return 0;
        }
        if (errno != ENOMEM) {
            return NOODLES_SetErrno("surface allocation");
        }
        if (!collected_after_drain) {
            size_t collected;
            if (NOODLES_DrainLink(data) < 0) {
                return -1;
            }
            if (noodles_surface_collect(data->link, &collected) < 0) {
                return NOODLES_SetErrno("surface collection");
            }
            collected_after_drain = SDL_TRUE;
            continue;
        }
        candidate = NOODLES_FindEvictionCandidate(data, surface);
        if (!candidate) {
            return NOODLES_SetErrno("surface allocation");
        }
        if (NOODLES_EvictSurface(data, candidate) < 0) {
            return -1;
        }
    }
}

static int NOODLES_EnsureGPU(NOODLES_RenderData *data, NOODLES_TextureData *surface)
{
    noodles_rect_t rect;
    if (NOODLES_MakeResident(data, surface) < 0) {
        return -1;
    }
    if (surface->gpu_valid) {
        return 0;
    }
    if (!surface->cpu_valid) {
        return SDL_SetError("Noodles surface has no current contents");
    }
    rect.x = 0;
    rect.y = 0;
    rect.width = (Uint32)surface->shadow->w;
    rect.height = (Uint32)surface->shadow->h;
    if (noodles_surface_update(surface->surface, &rect, surface->shadow->pixels,
                               (size_t)surface->shadow->pitch,
                               NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
        return NOODLES_SetErrno("fallback upload");
    }
    surface->gpu_valid = SDL_TRUE;
    return 0;
}

static void NOODLES_MarkGPUWrite(NOODLES_TextureData *surface)
{
    surface->gpu_valid = SDL_TRUE;
    surface->cpu_valid = SDL_FALSE;
}

static void NOODLES_MarkCPUWrite(NOODLES_TextureData *surface)
{
    surface->cpu_valid = SDL_TRUE;
    surface->gpu_valid = SDL_FALSE;
}

static int NOODLES_SubmitFill(NOODLES_RenderData *data, noodles_surface_t *target,
                              const noodles_rect_t *rect, Uint32 color)
{
    if (noodles_surface_fill(target, rect, color) == 0) {
        return 0;
    }
    if (errno == EAGAIN) {
        if (NOODLES_DrainLink(data) < 0) {
            return -1;
        }
        if (noodles_surface_fill(target, rect, color) == 0) {
            return 0;
        }
    }
    return NOODLES_SetErrno("fill");
}

static int NOODLES_SubmitDraw(NOODLES_RenderData *data, noodles_surface_t *target,
                              const noodles_surface_draw_t *draw)
{
    if (noodles_surface_draw_batch(data->link, target, draw, 1) == 0) {
        return 0;
    }
    if (errno == EAGAIN) {
        if (NOODLES_DrainLink(data) < 0) {
            return -1;
        }
        if (noodles_surface_draw_batch(data->link, target, draw, 1) == 0) {
            return 0;
        }
    }
    return NOODLES_SetErrno("draw");
}

static int NOODLES_GetOutputSize(SDL_Renderer *renderer, int *w, int *h)
{
    (void)renderer;
    *w = (int)NOODLES_BUFFER_WIDTH;
    *h = (int)NOODLES_BUFFER_HEIGHT;
    return 0;
}

static SDL_bool NOODLES_ValidFactor(SDL_BlendFactor factor)
{
    return factor >= SDL_BLENDFACTOR_ZERO && factor <= SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
}

static SDL_bool NOODLES_ValidOperation(SDL_BlendOperation operation)
{
    return operation >= SDL_BLENDOPERATION_ADD && operation <= SDL_BLENDOPERATION_MAXIMUM;
}

static SDL_bool NOODLES_SoftwareBlendMode(SDL_BlendMode blend)
{
    return blend == SDL_BLENDMODE_NONE || blend == SDL_BLENDMODE_BLEND ||
           blend == SDL_BLENDMODE_ADD || blend == SDL_BLENDMODE_MOD ||
           blend == SDL_BLENDMODE_MUL;
}

static SDL_bool NOODLES_SupportsBlendMode(SDL_Renderer *renderer, SDL_BlendMode blend)
{
    (void)renderer;
    return NOODLES_ValidFactor(SDL_GetBlendModeSrcColorFactor(blend)) &&
           NOODLES_ValidFactor(SDL_GetBlendModeDstColorFactor(blend)) &&
           NOODLES_ValidFactor(SDL_GetBlendModeSrcAlphaFactor(blend)) &&
           NOODLES_ValidFactor(SDL_GetBlendModeDstAlphaFactor(blend)) &&
           NOODLES_ValidOperation(SDL_GetBlendModeColorOperation(blend)) &&
           NOODLES_ValidOperation(SDL_GetBlendModeAlphaOperation(blend));
}

static int NOODLES_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata;

    if (texture->format != NOODLES_NATIVE_FORMAT) {
        return SDL_SetError("Noodles renderer requires SDL_PIXELFORMAT_ABGR8888 native textures");
    }

    texturedata = (NOODLES_TextureData *)SDL_calloc(1, sizeof(*texturedata));
    if (!texturedata) {
        return SDL_OutOfMemory();
    }
    if (NOODLES_InitSurface(data, texture->w, texture->h, SDL_FALSE,
                            texturedata) < 0) {
        SDL_free(texturedata);
        return -1;
    }

    texture->driverdata = texturedata;
    return 0;
}

static int NOODLES_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                                 const SDL_Rect *rect, const void *pixels, int pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    noodles_rect_t update_rect;
    Uint8 *destination;
    const Uint8 *source;
    int row;
    const SDL_bool gpu_was_valid = texturedata->gpu_valid;

    if (NOODLES_EnsureCPU(data, texturedata) < 0) {
        return -1;
    }
    destination = (Uint8 *)texturedata->shadow->pixels +
                  rect->y * texturedata->shadow->pitch + rect->x * 4;
    source = (const Uint8 *)pixels;
    for (row = 0; row < rect->h; ++row) {
        SDL_memcpy(destination, source, (size_t)rect->w * 4u);
        destination += texturedata->shadow->pitch;
        source += pitch;
    }

    texturedata->cpu_valid = SDL_TRUE;
    texturedata->gpu_valid = SDL_FALSE;
    if (texturedata->surface && gpu_was_valid) {
        update_rect.x = rect->x;
        update_rect.y = rect->y;
        update_rect.width = (Uint32)rect->w;
        update_rect.height = (Uint32)rect->h;
        if (noodles_surface_update(texturedata->surface, &update_rect, pixels,
                                   (size_t)pitch, NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
            return NOODLES_SetErrno("texture update");
        }
        texturedata->gpu_valid = SDL_TRUE;
        NOODLES_TouchSurface(data, texturedata);
    }
    return 0;
}

static int NOODLES_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                               const SDL_Rect *rect, void **pixels, int *pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;

    if (texturedata->locked) {
        return SDL_SetError("Noodles texture is not lockable");
    }
    if (NOODLES_EnsureCPU(data, texturedata) < 0) {
        return -1;
    }
    texturedata->lock_rect = *rect;
    texturedata->locked = SDL_TRUE;
    *pixels = (Uint8 *)texturedata->shadow->pixels +
              rect->y * texturedata->shadow->pitch + rect->x * 4;
    *pitch = texturedata->shadow->pitch;
    return 0;
}

static void NOODLES_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    noodles_rect_t rect;
    const SDL_bool gpu_was_valid = texturedata->gpu_valid;
    const void *pixels;
    size_t pitch;

    if (!texturedata->locked) {
        return;
    }
    texturedata->locked = SDL_FALSE;
    texturedata->cpu_valid = SDL_TRUE;
    texturedata->gpu_valid = SDL_FALSE;
    if (!texturedata->surface) {
        return;
    }
    if (gpu_was_valid) {
        rect.x = texturedata->lock_rect.x;
        rect.y = texturedata->lock_rect.y;
        rect.width = (Uint32)texturedata->lock_rect.w;
        rect.height = (Uint32)texturedata->lock_rect.h;
        pixels = (const Uint8 *)texturedata->shadow->pixels +
                 texturedata->lock_rect.y * texturedata->shadow->pitch +
                 texturedata->lock_rect.x * 4;
        pitch = (size_t)texturedata->shadow->pitch;
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.width = (Uint32)texturedata->shadow->w;
        rect.height = (Uint32)texturedata->shadow->h;
        pixels = texturedata->shadow->pixels;
        pitch = (size_t)texturedata->shadow->pitch;
    }
    if (noodles_surface_update(texturedata->surface, &rect, pixels, pitch,
                               NOODLES_DEFAULT_TIMEOUT_MS) == 0) {
        texturedata->gpu_valid = SDL_TRUE;
        NOODLES_TouchSurface(data, texturedata);
    } else {
        (void)NOODLES_SetErrno("texture unlock");
    }
}

static void NOODLES_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture,
                                        SDL_ScaleMode scaleMode)
{
    (void)renderer;
    (void)texture;
    (void)scaleMode;
}

static int NOODLES_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    if (texture) {
        NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
        data->target = texturedata;
    } else {
        data->target = &data->composition;
    }
    return 0;
}

static int NOODLES_QueueNoOp(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    (void)renderer;
    (void)cmd;
    return 0;
}

static int NOODLES_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                   const SDL_FPoint *points, int count)
{
    SDL_Point *queued = (SDL_Point *)SDL_AllocateRenderVertices(
        renderer, (size_t)count * sizeof(*queued), 0, &cmd->data.draw.first);
    int i;
    if (!queued) {
        return -1;
    }
    cmd->data.draw.count = (size_t)count;
    for (i = 0; i < count; ++i) {
        queued[i].x = (int)points[i].x;
        queued[i].y = (int)points[i].y;
    }
    return 0;
}

static int NOODLES_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                  const SDL_FRect *rects, int count)
{
    SDL_Rect *queued = (SDL_Rect *)SDL_AllocateRenderVertices(renderer,
                                                              (size_t)count * sizeof(*queued),
                                                              0, &cmd->data.draw.first);
    int i;
    if (!queued) {
        return -1;
    }
    cmd->data.draw.count = (size_t)count;
    for (i = 0; i < count; ++i) {
        queued[i].x = (int)rects[i].x;
        queued[i].y = (int)rects[i].y;
        queued[i].w = SDL_max((int)rects[i].w, 1);
        queued[i].h = SDL_max((int)rects[i].h, 1);
    }
    return 0;
}

static int NOODLES_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                             SDL_Texture *texture, const SDL_Rect *srcrect,
                             const SDL_FRect *dstrect)
{
    SDL_Rect *queued = (SDL_Rect *)SDL_AllocateRenderVertices(renderer, 2 * sizeof(*queued),
                                                              0, &cmd->data.draw.first);
    (void)texture;
    if (!queued) {
        return -1;
    }
    queued[0] = *srcrect;
    queued[1].x = (int)dstrect->x;
    queued[1].y = (int)dstrect->y;
    queued[1].w = (int)dstrect->w;
    queued[1].h = (int)dstrect->h;
    cmd->data.draw.count = 1;
    return 0;
}

static int NOODLES_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                               SDL_Texture *texture, const SDL_Rect *srcrect,
                               const SDL_FRect *dstrect, double angle,
                               const SDL_FPoint *center, SDL_RendererFlip flip,
                               float scale_x, float scale_y)
{
    NOODLES_CopyExData *queued = (NOODLES_CopyExData *)SDL_AllocateRenderVertices(
        renderer, sizeof(*queued), 0, &cmd->data.draw.first);
    (void)texture;
    (void)center;
    if (!queued) {
        return -1;
    }
    queued->srcrect = *srcrect;
    queued->dstrect.x = (int)dstrect->x;
    queued->dstrect.y = (int)dstrect->y;
    queued->dstrect.w = (int)dstrect->w;
    queued->dstrect.h = (int)dstrect->h;
    queued->angle = angle;
    queued->flip = flip;
    queued->scale_x = scale_x;
    queued->scale_y = scale_y;
    cmd->data.draw.count = 1;
    return 0;
}

static int NOODLES_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                 SDL_Texture *texture, const float *xy,
                                 int xy_stride, const SDL_Color *color,
                                 int color_stride, const float *uv, int uv_stride,
                                 int num_vertices, const void *indices,
                                 int num_indices, int size_indices,
                                 float scale_x, float scale_y)
{
    const int count = indices ? num_indices : num_vertices;
    const size_t element_size = texture ? sizeof(NOODLES_GeometryCopyData)
                                        : sizeof(NOODLES_GeometryFillData);
    Uint8 *queued = (Uint8 *)SDL_AllocateRenderVertices(
        renderer, (size_t)count * element_size, 0, &cmd->data.draw.first);
    int i;
    if (!queued) {
        return -1;
    }
    cmd->data.draw.count = (size_t)count;
    for (i = 0; i < count; ++i) {
        int index = i;
        const float *position;
        SDL_Color vertex_color;
        if (indices) {
            if (size_indices == 4) index = (int)((const Uint32 *)indices)[i];
            else if (size_indices == 2) index = (int)((const Uint16 *)indices)[i];
            else index = (int)((const Uint8 *)indices)[i];
        }
        vertex_color = *(const SDL_Color *)((const Uint8 *)color + index * color_stride);
        position = (const float *)((const Uint8 *)xy + index * xy_stride);
        if (texture) {
            const float *coordinate = (const float *)((const Uint8 *)uv + index * uv_stride);
            NOODLES_GeometryCopyData *vertex =
                &((NOODLES_GeometryCopyData *)queued)[i];
            vertex->source.x = (int)(coordinate[0] * texture->w);
            vertex->source.y = (int)(coordinate[1] * texture->h);
            vertex->destination.x = (int)(position[0] * scale_x);
            vertex->destination.y = (int)(position[1] * scale_y);
            trianglepoint_2_fixedpoint(&vertex->destination);
            vertex->color = vertex_color;
        } else {
            NOODLES_GeometryFillData *vertex =
                &((NOODLES_GeometryFillData *)queued)[i];
            vertex->destination.x = (int)(position[0] * scale_x);
            vertex->destination.y = (int)(position[1] * scale_y);
            trianglepoint_2_fixedpoint(&vertex->destination);
            vertex->color = vertex_color;
        }
    }
    return 0;
}

static SDL_bool NOODLES_EffectiveClip(const NOODLES_TextureData *target,
                                      const SDL_Rect *viewport,
                                      const SDL_Rect *clip, SDL_bool clip_enabled,
                                      SDL_Rect *result)
{
    SDL_Rect bounds;
    SDL_Rect translated;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = target->shadow->w;
    bounds.h = target->shadow->h;
    if (!SDL_IntersectRect(&bounds, viewport, result)) {
        return SDL_FALSE;
    }
    if (!clip_enabled) {
        return SDL_TRUE;
    }
    translated = *clip;
    translated.x += viewport->x;
    translated.y += viewport->y;
    return SDL_IntersectRect(result, &translated, result);
}

static SDL_bool NOODLES_ClipDraw(SDL_Rect *source, SDL_Rect *destination,
                                 const SDL_Rect *clip, SDL_RendererFlip flip)
{
    SDL_Rect visible;
    int left, top, right, bottom;
    if (!SDL_IntersectRect(destination, clip, &visible)) {
        return SDL_FALSE;
    }
    left = visible.x - destination->x;
    top = visible.y - destination->y;
    right = destination->x + destination->w - (visible.x + visible.w);
    bottom = destination->y + destination->h - (visible.y + visible.h);
    source->x += (flip & SDL_FLIP_HORIZONTAL) ? right : left;
    source->y += (flip & SDL_FLIP_VERTICAL) ? bottom : top;
    source->w = visible.w;
    source->h = visible.h;
    *destination = visible;
    return SDL_TRUE;
}

static Uint32 NOODLES_BlendFlags(SDL_BlendMode blend)
{
    if (blend == SDL_BLENDMODE_NONE) {
        return 0;
    }
    if (blend == SDL_BLENDMODE_BLEND) {
        return NOODLES_DRAW_BLEND;
    }
    if (blend == SDL_BLENDMODE_ADD) {
        return NOODLES_DRAW_MODE_ADD;
    }
    if (blend == SDL_BLENDMODE_MOD) {
        return NOODLES_DRAW_MODE_MOD;
    }
    if (blend == SDL_BLENDMODE_MUL) {
        return NOODLES_DRAW_MODE_MUL;
    }
    return NOODLES_DRAW_BLEND_MODE(SDL_GetBlendModeSrcColorFactor(blend),
                                   SDL_GetBlendModeDstColorFactor(blend),
                                   SDL_GetBlendModeColorOperation(blend),
                                   SDL_GetBlendModeSrcAlphaFactor(blend),
                                   SDL_GetBlendModeDstAlphaFactor(blend),
                                   SDL_GetBlendModeAlphaOperation(blend));
}

static int NOODLES_SoftwareCopy(NOODLES_RenderData *data, NOODLES_TextureData *target,
                                const SDL_RenderCommand *cmd, SDL_Rect source,
                                SDL_Rect destination, SDL_RendererFlip flip,
                                const SDL_Rect *viewport, const SDL_Rect *clip,
                                SDL_bool clip_enabled)
{
    NOODLES_TextureData *texturedata =
        (NOODLES_TextureData *)cmd->data.draw.texture->driverdata;
    SDL_Surface *copy_source = texturedata->shadow;
    SDL_Surface *temporary = NULL;
    SDL_Rect effective_clip;
    int result;

    if (NOODLES_EnsureCPU(data, texturedata) < 0 ||
        NOODLES_EnsureCPU(data, target) < 0) {
        return -1;
    }
    if (!NOODLES_SoftwareBlendMode(cmd->data.draw.blend)) {
        return SDL_SetError("Noodles CPU fallback cannot apply a custom blend mode");
    }
    destination.x += viewport->x;
    destination.y += viewport->y;
    if (!NOODLES_EffectiveClip(target, viewport, clip, clip_enabled,
                               &effective_clip)) {
        return 0;
    }
    SDL_SetClipRect(target->shadow, &effective_clip);

    if (flip != SDL_FLIP_NONE || source.w != destination.w ||
        source.h != destination.h || texturedata == target) {
        int x, y;
        SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(0, destination.w,
                                                             destination.h, 32,
                                                             NOODLES_NATIVE_FORMAT);
        if (!scaled) {
            return SDL_OutOfMemory();
        }
        if (flip != SDL_FLIP_NONE) {
            temporary = SDL_CreateRGBSurfaceWithFormat(0, destination.w,
                                                       destination.h, 32,
                                                       NOODLES_NATIVE_FORMAT);
        }
        if (flip != SDL_FLIP_NONE && !temporary) {
            SDL_FreeSurface(scaled);
            return SDL_OutOfMemory();
        }
        SDL_SetSurfaceBlendMode(copy_source, SDL_BLENDMODE_NONE);
        SDL_SetSurfaceColorMod(copy_source, 255, 255, 255);
        SDL_SetSurfaceAlphaMod(copy_source, 255);
        {
            SDL_Rect scaled_rect = { 0, 0, destination.w, destination.h };
            if (SDL_PrivateUpperBlitScaled(copy_source, &source, scaled, &scaled_rect,
                                           cmd->data.draw.texture->scaleMode) < 0) {
                SDL_FreeSurface(scaled);
                SDL_FreeSurface(temporary);
                return -1;
            }
        }
        if (flip != SDL_FLIP_NONE) {
            for (y = 0; y < destination.h; ++y) {
                const int source_y = (flip & SDL_FLIP_VERTICAL) ? destination.h - 1 - y : y;
                const Uint32 *source_row = (const Uint32 *)((const Uint8 *)scaled->pixels +
                                                            source_y * scaled->pitch);
                Uint32 *destination_row = (Uint32 *)((Uint8 *)temporary->pixels +
                                                     y * temporary->pitch);
                for (x = 0; x < destination.w; ++x) {
                    const int source_x = (flip & SDL_FLIP_HORIZONTAL)
                                       ? destination.w - 1 - x : x;
                    destination_row[x] = source_row[source_x];
                }
            }
            SDL_FreeSurface(scaled);
        } else {
            temporary = scaled;
        }
        copy_source = temporary;
        source.x = 0;
        source.y = 0;
        source.w = destination.w;
        source.h = destination.h;
    }

    SDL_SetSurfaceColorMod(copy_source, cmd->data.draw.r, cmd->data.draw.g,
                           cmd->data.draw.b);
    SDL_SetSurfaceAlphaMod(copy_source, cmd->data.draw.a);
    if (SDL_SetSurfaceBlendMode(copy_source, cmd->data.draw.blend) < 0) {
        SDL_FreeSurface(temporary);
        return -1;
    }
    result = SDL_BlitSurface(copy_source, &source, target->shadow, &destination);
    SDL_FreeSurface(temporary);
    if (result < 0) {
        return -1;
    }
    NOODLES_MarkCPUWrite(target);
    return 0;
}

static int NOODLES_RunCopy(NOODLES_RenderData *data, NOODLES_TextureData *target,
                           const SDL_RenderCommand *cmd, SDL_Rect source,
                           SDL_Rect destination, SDL_RendererFlip flip,
                           const SDL_Rect *viewport, const SDL_Rect *clip,
                           SDL_bool clip_enabled)
{
    NOODLES_TextureData *texturedata =
        (NOODLES_TextureData *)cmd->data.draw.texture->driverdata;
    noodles_surface_draw_t draw;
    SDL_Rect effective_clip;
    Uint32 flags;
    Uint32 modulation;

    if (source.w != destination.w || source.h != destination.h ||
        texturedata == target) {
        return NOODLES_SoftwareCopy(data, target, cmd, source, destination, flip,
                                    viewport, clip, clip_enabled);
    }
    destination.x += viewport->x;
    destination.y += viewport->y;
    if (!NOODLES_EffectiveClip(target, viewport, clip, clip_enabled, &effective_clip) ||
        !NOODLES_ClipDraw(&source, &destination, &effective_clip, flip)) {
        return 0;
    }

    if (NOODLES_EnsureGPU(data, target) < 0 ||
        NOODLES_EnsureGPU(data, texturedata) < 0) {
        return -1;
    }

    flags = NOODLES_BlendFlags(cmd->data.draw.blend);
    if (flip & SDL_FLIP_HORIZONTAL) {
        flags |= NOODLES_DRAW_MIRROR_X;
    }
    if (flip & SDL_FLIP_VERTICAL) {
        flags |= NOODLES_DRAW_MIRROR_Y;
    }
    modulation = NOODLES_PackRGBA(cmd->data.draw.r, cmd->data.draw.g,
                                  cmd->data.draw.b, cmd->data.draw.a);
    if (cmd->data.draw.blend == SDL_BLENDMODE_NONE && flags == 0 &&
        modulation != 0xffffffffu) {
        flags = NOODLES_DRAW_BLEND_MODE(NOODLES_BLENDFACTOR_ONE,
                                        NOODLES_BLENDFACTOR_ZERO,
                                        NOODLES_BLENDOP_ADD,
                                        NOODLES_BLENDFACTOR_ONE,
                                        NOODLES_BLENDFACTOR_ZERO,
                                        NOODLES_BLENDOP_ADD);
    }

    draw.source = texturedata->surface;
    draw.source_rect.x = source.x;
    draw.source_rect.y = source.y;
    draw.source_rect.width = (Uint32)source.w;
    draw.source_rect.height = (Uint32)source.h;
    draw.dst_x = destination.x;
    draw.dst_y = destination.y;
    draw.flags = flags;
    draw.modulation = modulation;
    if (NOODLES_SubmitDraw(data, target->surface, &draw) < 0) {
        return -1;
    }
    NOODLES_MarkGPUWrite(target);
    return 0;
}

static int NOODLES_RunCommandQueueImpl(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                       void *vertices, size_t vertsize)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *target = data->target;
    SDL_Rect viewport = { 0, 0, target->shadow->w, target->shadow->h };
    SDL_Rect clip = { 0, 0, 0, 0 };
    SDL_bool clip_enabled = SDL_FALSE;
    (void)vertsize;

    while (cmd) {
        switch (cmd->command) {
        case SDL_RENDERCMD_NO_OP:
        case SDL_RENDERCMD_SETDRAWCOLOR:
            break;
        case SDL_RENDERCMD_SETVIEWPORT:
            viewport = cmd->data.viewport.rect;
            break;
        case SDL_RENDERCMD_SETCLIPRECT:
            clip = cmd->data.cliprect.rect;
            clip_enabled = cmd->data.cliprect.enabled;
            break;
        case SDL_RENDERCMD_CLEAR: {
            noodles_rect_t rect;
            if (NOODLES_MakeResident(data, target) < 0) {
                return -1;
            }
            rect.x = 0;
            rect.y = 0;
            rect.width = (Uint32)target->shadow->w;
            rect.height = (Uint32)target->shadow->h;
            if (NOODLES_SubmitFill(data, target->surface, &rect,
                    NOODLES_PackRGBA(cmd->data.color.r, cmd->data.color.g,
                                     cmd->data.color.b, cmd->data.color.a)) < 0) {
                return -1;
            }
            NOODLES_MarkGPUWrite(target);
            break;
        }
        case SDL_RENDERCMD_FILL_RECTS: {
            SDL_Rect *rects = (SDL_Rect *)((Uint8 *)vertices + cmd->data.draw.first);
            SDL_Rect effective_clip;
            size_t i;
            if (!NOODLES_EffectiveClip(target, &viewport, &clip, clip_enabled,
                                       &effective_clip)) {
                break;
            }
            if (cmd->data.draw.blend != SDL_BLENDMODE_NONE) {
                if (!NOODLES_SoftwareBlendMode(cmd->data.draw.blend)) {
                    return SDL_SetError("Noodles CPU fill fallback cannot apply a custom blend mode");
                }
                if (NOODLES_EnsureCPU(data, target) < 0) {
                    return -1;
                }
                SDL_SetClipRect(target->shadow, &effective_clip);
                for (i = 0; i < cmd->data.draw.count; ++i) {
                    SDL_Rect destination = rects[i];
                    destination.x += viewport.x;
                    destination.y += viewport.y;
                    if (SDL_BlendFillRect(target->shadow, &destination,
                                          cmd->data.draw.blend,
                                          cmd->data.draw.r, cmd->data.draw.g,
                                          cmd->data.draw.b, cmd->data.draw.a) < 0) {
                        return -1;
                    }
                }
                NOODLES_MarkCPUWrite(target);
                break;
            }
            if (NOODLES_EnsureGPU(data, target) < 0) {
                return -1;
            }
            for (i = 0; i < cmd->data.draw.count; ++i) {
                SDL_Rect destination = rects[i];
                SDL_Rect visible;
                noodles_rect_t fill;
                destination.x += viewport.x;
                destination.y += viewport.y;
                if (!SDL_IntersectRect(&destination, &effective_clip, &visible)) {
                    continue;
                }
                fill.x = visible.x;
                fill.y = visible.y;
                fill.width = (Uint32)visible.w;
                fill.height = (Uint32)visible.h;
                if (NOODLES_SubmitFill(data, target->surface, &fill,
                        NOODLES_PackRGBA(cmd->data.draw.r, cmd->data.draw.g,
                                         cmd->data.draw.b, cmd->data.draw.a)) < 0) {
                    return -1;
                }
            }
            NOODLES_MarkGPUWrite(target);
            break;
        }
        case SDL_RENDERCMD_DRAW_POINTS:
        case SDL_RENDERCMD_DRAW_LINES: {
            SDL_Point *points = (SDL_Point *)((Uint8 *)vertices + cmd->data.draw.first);
            SDL_Rect effective_clip;
            const int count = (int)cmd->data.draw.count;
            int i;
            int result;
            if (!NOODLES_SoftwareBlendMode(cmd->data.draw.blend)) {
                return SDL_SetError("Noodles CPU primitive fallback cannot apply a custom blend mode");
            }
            if (NOODLES_EnsureCPU(data, target) < 0) {
                return -1;
            }
            if (!NOODLES_EffectiveClip(target, &viewport, &clip, clip_enabled,
                                       &effective_clip)) {
                break;
            }
            SDL_SetClipRect(target->shadow, &effective_clip);
            for (i = 0; i < count; ++i) {
                points[i].x += viewport.x;
                points[i].y += viewport.y;
            }
            if (cmd->command == SDL_RENDERCMD_DRAW_POINTS) {
                result = cmd->data.draw.blend == SDL_BLENDMODE_NONE
                       ? SDL_DrawPoints(target->shadow, points, count,
                             SDL_MapRGBA(target->shadow->format, cmd->data.draw.r,
                                         cmd->data.draw.g, cmd->data.draw.b,
                                         cmd->data.draw.a))
                       : SDL_BlendPoints(target->shadow, points, count,
                                         cmd->data.draw.blend, cmd->data.draw.r,
                                         cmd->data.draw.g, cmd->data.draw.b,
                                         cmd->data.draw.a);
            } else {
                result = cmd->data.draw.blend == SDL_BLENDMODE_NONE
                       ? SDL_DrawLines(target->shadow, points, count,
                             SDL_MapRGBA(target->shadow->format, cmd->data.draw.r,
                                         cmd->data.draw.g, cmd->data.draw.b,
                                         cmd->data.draw.a))
                       : SDL_BlendLines(target->shadow, points, count,
                                        cmd->data.draw.blend, cmd->data.draw.r,
                                        cmd->data.draw.g, cmd->data.draw.b,
                                        cmd->data.draw.a);
            }
            if (result < 0) {
                return -1;
            }
            NOODLES_MarkCPUWrite(target);
            break;
        }
        case SDL_RENDERCMD_COPY: {
            SDL_Rect *queued = (SDL_Rect *)((Uint8 *)vertices + cmd->data.draw.first);
            if (NOODLES_RunCopy(data, target, cmd, queued[0], queued[1], SDL_FLIP_NONE,
                                &viewport, &clip, clip_enabled) < 0) {
                return -1;
            }
            break;
        }
        case SDL_RENDERCMD_COPY_EX: {
            NOODLES_CopyExData *queued =
                (NOODLES_CopyExData *)((Uint8 *)vertices + cmd->data.draw.first);
            if (queued->angle != 0.0) {
                return SDL_SetError("Noodles rotation fallback is not implemented");
            }
            queued->dstrect.x = (int)((float)queued->dstrect.x * queued->scale_x);
            queued->dstrect.y = (int)((float)queued->dstrect.y * queued->scale_y);
            queued->dstrect.w = (int)((float)queued->dstrect.w * queued->scale_x);
            queued->dstrect.h = (int)((float)queued->dstrect.h * queued->scale_y);
            if (NOODLES_RunCopy(data, target, cmd, queued->srcrect, queued->dstrect,
                                queued->flip, &viewport, &clip, clip_enabled) < 0) {
                return -1;
            }
            break;
        }
        case SDL_RENDERCMD_GEOMETRY: {
            const int count = (int)cmd->data.draw.count;
            SDL_Rect effective_clip;
            int i;
            if (!NOODLES_SoftwareBlendMode(cmd->data.draw.blend)) {
                return SDL_SetError("Noodles CPU geometry fallback cannot apply a custom blend mode");
            }
            if (NOODLES_EnsureCPU(data, target) < 0) {
                return -1;
            }
            if (!NOODLES_EffectiveClip(target, &viewport, &clip, clip_enabled,
                                       &effective_clip)) {
                break;
            }
            SDL_SetClipRect(target->shadow, &effective_clip);
            if (cmd->data.draw.texture) {
                NOODLES_TextureData *texturedata =
                    (NOODLES_TextureData *)cmd->data.draw.texture->driverdata;
                NOODLES_GeometryCopyData *geometry =
                    (NOODLES_GeometryCopyData *)((Uint8 *)vertices + cmd->data.draw.first);
                SDL_Point fixed_viewport = { viewport.x, viewport.y };
                if (NOODLES_EnsureCPU(data, texturedata) < 0) {
                    return -1;
                }
                trianglepoint_2_fixedpoint(&fixed_viewport);
                SDL_SetSurfaceColorMod(texturedata->shadow, cmd->data.draw.r,
                                       cmd->data.draw.g, cmd->data.draw.b);
                SDL_SetSurfaceAlphaMod(texturedata->shadow, cmd->data.draw.a);
                if (SDL_SetSurfaceBlendMode(texturedata->shadow,
                                            cmd->data.draw.blend) < 0) {
                    return -1;
                }
                for (i = 0; i < count; ++i) {
                    geometry[i].destination.x += fixed_viewport.x;
                    geometry[i].destination.y += fixed_viewport.y;
                }
                for (i = 0; i < count; i += 3) {
                    if (SDL_SW_BlitTriangle(texturedata->shadow,
                            &geometry[i].source, &geometry[i + 1].source,
                            &geometry[i + 2].source, target->shadow,
                            &geometry[i].destination, &geometry[i + 1].destination,
                            &geometry[i + 2].destination, geometry[i].color,
                            geometry[i + 1].color, geometry[i + 2].color) < 0) {
                        return -1;
                    }
                }
            } else {
                NOODLES_GeometryFillData *geometry =
                    (NOODLES_GeometryFillData *)((Uint8 *)vertices + cmd->data.draw.first);
                SDL_Point fixed_viewport = { viewport.x, viewport.y };
                trianglepoint_2_fixedpoint(&fixed_viewport);
                for (i = 0; i < count; ++i) {
                    geometry[i].destination.x += fixed_viewport.x;
                    geometry[i].destination.y += fixed_viewport.y;
                }
                for (i = 0; i < count; i += 3) {
                    if (SDL_SW_FillTriangle(target->shadow,
                            &geometry[i].destination, &geometry[i + 1].destination,
                            &geometry[i + 2].destination, cmd->data.draw.blend,
                            geometry[i].color, geometry[i + 1].color,
                            geometry[i + 2].color) < 0) {
                        return -1;
                    }
                }
            }
            NOODLES_MarkCPUWrite(target);
            break;
        }
        }
        cmd = cmd->next;
    }
    return 0;
}

static int NOODLES_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                   void *vertices, size_t vertsize)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    Uint64 start = 0;
    int result;
    if (data->stats_enabled) {
        start = SDL_GetPerformanceCounter();
    }
    result = NOODLES_RunCommandQueueImpl(renderer, cmd, vertices, vertsize);
    if (data->stats_enabled) {
        const Uint64 elapsed = SDL_GetPerformanceCounter() - start;
        data->stats_queue_ticks += elapsed;
        data->stats_queue_max_ticks = SDL_max(data->stats_queue_max_ticks, elapsed);
    }
    return result;
}

static int NOODLES_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                                    Uint32 format, void *pixels, int pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint8 *source;
    int row;
    if (NOODLES_EnsureCPU(data, data->target) < 0) {
        return -1;
    }
    source = (const Uint8 *)data->target->shadow->pixels +
             rect->y * data->target->shadow->pitch + rect->x * 4;
    if (format != NOODLES_NATIVE_FORMAT) {
        return SDL_ConvertPixels(rect->w, rect->h, NOODLES_NATIVE_FORMAT,
                                 source, data->target->shadow->pitch,
                                 format, pixels, pitch);
    }
    for (row = 0; row < rect->h; ++row) {
        SDL_memcpy((Uint8 *)pixels + row * pitch,
                   source + row * data->target->shadow->pitch,
                   (size_t)rect->w * 4u);
    }
    return 0;
}

static int NOODLES_RenderPresent(SDL_Renderer *renderer)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    noodles_rect_t source;
    source.x = 0;
    source.y = 0;
    source.width = NOODLES_BUFFER_WIDTH;
    source.height = NOODLES_BUFFER_HEIGHT;
    if (NOODLES_EnsureGPU(data, &data->composition) < 0) {
        return -1;
    }
    if (noodles_surface_blit_to_back_buffer(data->link, 0, 0, data->composition.surface,
                                            &source) < 0) {
        if (errno != EAGAIN || NOODLES_DrainLink(data) < 0 ||
            noodles_surface_blit_to_back_buffer(data->link, 0, 0, data->composition.surface,
                                                 &source) < 0) {
            return NOODLES_SetErrno("present copy");
        }
    }
    if (noodles_present_and_wait(data->link) < 0) {
        return NOODLES_SetErrno("present");
    }
    if (data->stats_enabled) {
        const Uint64 now = SDL_GetPerformanceCounter();
        const Uint64 frequency = SDL_GetPerformanceFrequency();
        const Uint64 present_ticks = now - start;
        const Uint64 interval_ticks = now - data->stats_start;
        data->stats_frames++;
        data->stats_present_ticks += present_ticks;
        data->stats_present_max_ticks = SDL_max(data->stats_present_max_ticks,
                                                present_ticks);
        if (interval_ticks >= frequency * 5u) {
            const double milliseconds = 1000.0 / (double)frequency;
            const double frame_count = (double)data->stats_frames;
            double other_ticks = (double)interval_ticks -
                                 (double)data->stats_queue_ticks -
                                 (double)data->stats_present_ticks;
            if (other_ticks < 0.0) {
                other_ticks = 0.0;
            }
            SDL_Log("Noodles stats: frames=%llu fps=%.2f queue=%.3f ms avg/%.3f max present=%.3f ms avg/%.3f max other=%.3f ms avg",
                    (unsigned long long)data->stats_frames,
                    frame_count * (double)frequency / (double)interval_ticks,
                    (double)data->stats_queue_ticks * milliseconds / frame_count,
                    (double)data->stats_queue_max_ticks * milliseconds,
                    (double)data->stats_present_ticks * milliseconds / frame_count,
                    (double)data->stats_present_max_ticks * milliseconds,
                    other_ticks * milliseconds / frame_count);
            data->stats_start = now;
            data->stats_frames = 0;
            data->stats_queue_ticks = 0;
            data->stats_queue_max_ticks = 0;
            data->stats_present_ticks = 0;
            data->stats_present_max_ticks = 0;
        }
    }
    return 0;
}

static void NOODLES_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    if (!texturedata) {
        return;
    }
    if (data->target == texturedata) {
        data->target = &data->composition;
    }
    NOODLES_FreeSurface(data, texturedata);
    SDL_free(texturedata);
    texture->driverdata = NULL;
}

static void NOODLES_DestroyRenderer(SDL_Renderer *renderer)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    if (!data) {
        return;
    }
    NOODLES_FreeSurface(data, &data->composition);
    if (data->link) {
        (void)noodles_link_close(data->link, NOODLES_DEFAULT_TIMEOUT_MS);
    }
    SDL_free(data);
    renderer->driverdata = NULL;
}

static int NOODLES_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, Uint32 flags)
{
    NOODLES_RenderData *data;
    noodles_device_info_t info;
    const char *budget_hint;
    int budget_mb = (int)NOODLES_DEFAULT_RESIDENT_MB;
    (void)window;
    (void)flags;

    data = (NOODLES_RenderData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }
    renderer->driverdata = data;
    budget_hint = SDL_GetHint("SDL_RENDER_NOODLES_RESIDENT_MB");
    if (budget_hint && *budget_hint) {
        const int configured = SDL_atoi(budget_hint);
        if (configured > 0) {
            budget_mb = SDL_max(8, SDL_min(configured,
                                           (int)NOODLES_MAX_RESIDENT_MB));
        }
    }
    data->resident_budget = (size_t)budget_mb * 1024u * 1024u;
    data->stats_enabled = SDL_getenv("SDL_RENDER_NOODLES_STATS") != NULL;
    if (data->stats_enabled) {
        data->stats_start = SDL_GetPerformanceCounter();
    }
    if (noodles_link_open(&data->link) < 0) {
        const int saved_errno = errno;
        NOODLES_DestroyRenderer(renderer);
        errno = saved_errno;
        return NOODLES_SetErrno("device open");
    }
    if (noodles_link_get_info(data->link, &info) < 0) {
        const int saved_errno = errno;
        NOODLES_DestroyRenderer(renderer);
        errno = saved_errno;
        return NOODLES_SetErrno("device query");
    }
    if (info.protocol_version < NOODLES_REQUIRED_PROTOCOL ||
        !(info.opcode_mask & NOODLES_CAP_BLIT_BLEND)) {
        NOODLES_DestroyRenderer(renderer);
        return SDL_SetError("Noodles renderer requires protocol 1.3");
    }
    if (NOODLES_InitSurface(data, NOODLES_BUFFER_WIDTH, NOODLES_BUFFER_HEIGHT,
                            SDL_TRUE, &data->composition) < 0 ||
        NOODLES_MakeResident(data, &data->composition) < 0) {
        NOODLES_DestroyRenderer(renderer);
        return -1;
    }
    data->target = &data->composition;

    renderer->GetOutputSize = NOODLES_GetOutputSize;
    renderer->SupportsBlendMode = NOODLES_SupportsBlendMode;
    renderer->CreateTexture = NOODLES_CreateTexture;
    renderer->UpdateTexture = NOODLES_UpdateTexture;
    renderer->LockTexture = NOODLES_LockTexture;
    renderer->UnlockTexture = NOODLES_UnlockTexture;
    renderer->SetTextureScaleMode = NOODLES_SetTextureScaleMode;
    renderer->SetRenderTarget = NOODLES_SetRenderTarget;
    renderer->QueueSetViewport = NOODLES_QueueNoOp;
    renderer->QueueSetDrawColor = NOODLES_QueueNoOp;
    renderer->QueueDrawPoints = NOODLES_QueueDrawPoints;
    renderer->QueueDrawLines = NOODLES_QueueDrawPoints;
    renderer->QueueFillRects = NOODLES_QueueFillRects;
    renderer->QueueCopy = NOODLES_QueueCopy;
    renderer->QueueCopyEx = NOODLES_QueueCopyEx;
    renderer->QueueGeometry = NOODLES_QueueGeometry;
    renderer->RunCommandQueue = NOODLES_RunCommandQueue;
    renderer->RenderReadPixels = NOODLES_RenderReadPixels;
    renderer->RenderPresent = NOODLES_RenderPresent;
    renderer->DestroyTexture = NOODLES_DestroyTexture;
    renderer->DestroyRenderer = NOODLES_DestroyRenderer;
    renderer->info = NOODLES_RenderDriver.info;
    renderer->always_batch = SDL_TRUE;
    return 0;
}

SDL_RenderDriver NOODLES_RenderDriver = {
    NOODLES_CreateRenderer,
    {
        "noodles",
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
        1,
        { NOODLES_NATIVE_FORMAT },
        4096,
        4096
    }
};

#endif /* SDL_VIDEO_RENDER_NOODLES */
