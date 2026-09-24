/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty. See SDL's zlib license for the complete terms.

  MiSTer-Noodles renderer. This first-stage backend intentionally accepts
  only operations implemented by protocol 1.3. CPU fallbacks are added in a
  later integration stage, after this path is independently qualified.
*/
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_RENDER_NOODLES

#include "SDL_render_noodles.h"

#include "SDL_error.h"
#include "SDL_hints.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_stdinc.h"

#include <errno.h>
#include <string.h>

#include <noodles_link.h>
#include <noodles_surface.h>

#define NOODLES_NATIVE_FORMAT SDL_PIXELFORMAT_ABGR8888
#define NOODLES_REQUIRED_PROTOCOL 0x00010003u

typedef struct NOODLES_TextureData
{
    noodles_surface_t *surface;
    Uint8 *lock_pixels;
    int lock_pitch;
    SDL_Rect lock_rect;
    SDL_bool locked;
} NOODLES_TextureData;

typedef struct NOODLES_RenderData
{
    noodles_link_t *link;
    noodles_surface_t *composition;
    noodles_surface_t *target;
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
    if (noodles_surface_create(data->link, (Uint32)texture->w, (Uint32)texture->h,
                               &texturedata->surface) < 0) {
        SDL_free(texturedata);
        return NOODLES_SetErrno("texture allocation");
    }

    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        size_t bytes;
        texturedata->lock_pitch = texture->w * 4;
        bytes = (size_t)texturedata->lock_pitch * (size_t)texture->h;
        texturedata->lock_pixels = (Uint8 *)SDL_malloc(bytes);
        if (!texturedata->lock_pixels) {
            noodles_surface_destroy(texturedata->surface);
            SDL_free(texturedata);
            return SDL_OutOfMemory();
        }
    }

    texture->driverdata = texturedata;
    return 0;
}

static int NOODLES_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                                 const SDL_Rect *rect, const void *pixels, int pitch)
{
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    noodles_rect_t update_rect;
    (void)renderer;

    update_rect.x = rect->x;
    update_rect.y = rect->y;
    update_rect.width = (Uint32)rect->w;
    update_rect.height = (Uint32)rect->h;
    if (noodles_surface_update(texturedata->surface, &update_rect, pixels, (size_t)pitch,
                               NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
        return NOODLES_SetErrno("texture update");
    }
    return 0;
}

static int NOODLES_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                               const SDL_Rect *rect, void **pixels, int *pitch)
{
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    (void)renderer;

    if (!texturedata->lock_pixels || texturedata->locked) {
        return SDL_SetError("Noodles texture is not lockable");
    }
    texturedata->lock_rect = *rect;
    texturedata->locked = SDL_TRUE;
    *pixels = texturedata->lock_pixels + rect->y * texturedata->lock_pitch + rect->x * 4;
    *pitch = texturedata->lock_pitch;
    return 0;
}

static void NOODLES_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    const SDL_Rect rect = texturedata->lock_rect;
    const Uint8 *pixels;
    (void)renderer;

    if (!texturedata->locked) {
        return;
    }
    pixels = texturedata->lock_pixels + rect.y * texturedata->lock_pitch + rect.x * 4;
    texturedata->locked = SDL_FALSE;
    (void)NOODLES_UpdateTexture(renderer, texture, &rect, pixels, texturedata->lock_pitch);
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
        data->target = texturedata->surface;
    } else {
        data->target = data->composition;
    }
    return 0;
}

static int NOODLES_QueueNoOp(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    (void)renderer;
    (void)cmd;
    return 0;
}

static int NOODLES_QueueUnsupportedPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                          const SDL_FPoint *points, int count)
{
    (void)renderer;
    (void)cmd;
    (void)points;
    (void)count;
    return SDL_Unsupported();
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

static int NOODLES_QueueUnsupportedGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                            SDL_Texture *texture, const float *xy,
                                            int xy_stride, const SDL_Color *color,
                                            int color_stride, const float *uv, int uv_stride,
                                            int num_vertices, const void *indices,
                                            int num_indices, int size_indices,
                                            float scale_x, float scale_y)
{
    (void)renderer;
    (void)cmd;
    (void)texture;
    (void)xy;
    (void)xy_stride;
    (void)color;
    (void)color_stride;
    (void)uv;
    (void)uv_stride;
    (void)num_vertices;
    (void)indices;
    (void)num_indices;
    (void)size_indices;
    (void)scale_x;
    (void)scale_y;
    return SDL_Unsupported();
}

static SDL_bool NOODLES_EffectiveClip(noodles_surface_t *target, const SDL_Rect *viewport,
                                      const SDL_Rect *clip, SDL_bool clip_enabled,
                                      SDL_Rect *result)
{
    SDL_Rect bounds;
    SDL_Rect translated;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = (int)noodles_surface_width(target);
    bounds.h = (int)noodles_surface_height(target);
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

static int NOODLES_RunCopy(NOODLES_RenderData *data, noodles_surface_t *target,
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

    if (source.w != destination.w || source.h != destination.h) {
        return SDL_SetError("Noodles scaled copy is not implemented");
    }
    destination.x += viewport->x;
    destination.y += viewport->y;
    if (!NOODLES_EffectiveClip(target, viewport, clip, clip_enabled, &effective_clip) ||
        !NOODLES_ClipDraw(&source, &destination, &effective_clip, flip)) {
        return 0;
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
    return NOODLES_SubmitDraw(data, target, &draw);
}

static int NOODLES_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                   void *vertices, size_t vertsize)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    noodles_surface_t *target = data->target;
    SDL_Rect viewport = { 0, 0, (int)noodles_surface_width(target),
                          (int)noodles_surface_height(target) };
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
            rect.x = 0;
            rect.y = 0;
            rect.width = noodles_surface_width(target);
            rect.height = noodles_surface_height(target);
            if (NOODLES_SubmitFill(data, target, &rect,
                    NOODLES_PackRGBA(cmd->data.color.r, cmd->data.color.g,
                                     cmd->data.color.b, cmd->data.color.a)) < 0) {
                return -1;
            }
            break;
        }
        case SDL_RENDERCMD_FILL_RECTS: {
            SDL_Rect *rects = (SDL_Rect *)((Uint8 *)vertices + cmd->data.draw.first);
            SDL_Rect effective_clip;
            size_t i;
            if (cmd->data.draw.blend != SDL_BLENDMODE_NONE) {
                return SDL_SetError("Noodles blended fill is not implemented");
            }
            if (!NOODLES_EffectiveClip(target, &viewport, &clip, clip_enabled,
                                       &effective_clip)) {
                break;
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
                if (NOODLES_SubmitFill(data, target, &fill,
                        NOODLES_PackRGBA(cmd->data.draw.r, cmd->data.draw.g,
                                         cmd->data.draw.b, cmd->data.draw.a)) < 0) {
                    return -1;
                }
            }
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
            if (queued->angle != 0.0 || queued->scale_x != 1.0f || queued->scale_y != 1.0f) {
                return SDL_SetError("Noodles rotation or renderer scaling is not implemented");
            }
            if (NOODLES_RunCopy(data, target, cmd, queued->srcrect, queued->dstrect,
                                queued->flip, &viewport, &clip, clip_enabled) < 0) {
                return -1;
            }
            break;
        }
        case SDL_RENDERCMD_DRAW_POINTS:
        case SDL_RENDERCMD_DRAW_LINES:
        case SDL_RENDERCMD_GEOMETRY:
            return SDL_SetError("Noodles primitive drawing is not implemented");
        }
        cmd = cmd->next;
    }
    return 0;
}

static int NOODLES_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                                    Uint32 format, void *pixels, int pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    noodles_rect_t source;
    Uint8 *temporary = NULL;
    void *destination = pixels;
    size_t destination_pitch = (size_t)pitch;

    source.x = rect->x;
    source.y = rect->y;
    source.width = (Uint32)rect->w;
    source.height = (Uint32)rect->h;
    if (format != NOODLES_NATIVE_FORMAT) {
        temporary = (Uint8 *)SDL_malloc((size_t)rect->w * 4u * (size_t)rect->h);
        if (!temporary) {
            return SDL_OutOfMemory();
        }
        destination = temporary;
        destination_pitch = (size_t)rect->w * 4u;
    }
    if (noodles_surface_read(data->target, &source, destination, destination_pitch,
                             NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
        SDL_free(temporary);
        return NOODLES_SetErrno("readback");
    }
    if (temporary) {
        const int result = SDL_ConvertPixels(rect->w, rect->h, NOODLES_NATIVE_FORMAT,
                                             temporary, (int)destination_pitch,
                                             format, pixels, pitch);
        SDL_free(temporary);
        return result;
    }
    return 0;
}

static int NOODLES_RenderPresent(SDL_Renderer *renderer)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    noodles_rect_t source;
    source.x = 0;
    source.y = 0;
    source.width = NOODLES_BUFFER_WIDTH;
    source.height = NOODLES_BUFFER_HEIGHT;
    if (noodles_surface_blit_to_back_buffer(data->link, 0, 0, data->composition,
                                            &source) < 0) {
        if (errno != EAGAIN || NOODLES_DrainLink(data) < 0 ||
            noodles_surface_blit_to_back_buffer(data->link, 0, 0, data->composition,
                                                 &source) < 0) {
            return NOODLES_SetErrno("present copy");
        }
    }
    if (noodles_present_and_wait(data->link) < 0) {
        return NOODLES_SetErrno("present");
    }
    return 0;
}

static void NOODLES_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    (void)renderer;
    if (!texturedata) {
        return;
    }
    if (texturedata->surface) {
        (void)noodles_surface_destroy(texturedata->surface);
    }
    SDL_free(texturedata->lock_pixels);
    SDL_free(texturedata);
    texture->driverdata = NULL;
}

static void NOODLES_DestroyRenderer(SDL_Renderer *renderer)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    if (!data) {
        return;
    }
    if (data->composition) {
        (void)noodles_surface_destroy(data->composition);
    }
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
    (void)window;
    (void)flags;

    data = (NOODLES_RenderData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }
    renderer->driverdata = data;
    if (noodles_link_open(&data->link) < 0) {
        NOODLES_DestroyRenderer(renderer);
        return NOODLES_SetErrno("device open");
    }
    if (noodles_link_get_info(data->link, &info) < 0) {
        NOODLES_DestroyRenderer(renderer);
        return NOODLES_SetErrno("device query");
    }
    if (info.protocol_version < NOODLES_REQUIRED_PROTOCOL ||
        !(info.opcode_mask & NOODLES_CAP_BLIT_BLEND)) {
        NOODLES_DestroyRenderer(renderer);
        return SDL_SetError("Noodles renderer requires protocol 1.3");
    }
    if (noodles_surface_create(data->link, NOODLES_BUFFER_WIDTH, NOODLES_BUFFER_HEIGHT,
                               &data->composition) < 0) {
        NOODLES_DestroyRenderer(renderer);
        return NOODLES_SetErrno("composition allocation");
    }
    data->target = data->composition;

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
    renderer->QueueDrawPoints = NOODLES_QueueUnsupportedPoints;
    renderer->QueueDrawLines = NOODLES_QueueUnsupportedPoints;
    renderer->QueueFillRects = NOODLES_QueueFillRects;
    renderer->QueueCopy = NOODLES_QueueCopy;
    renderer->QueueCopyEx = NOODLES_QueueCopyEx;
    renderer->QueueGeometry = NOODLES_QueueUnsupportedGeometry;
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
