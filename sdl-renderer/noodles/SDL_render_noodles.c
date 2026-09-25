/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty. See SDL's zlib license for the complete terms.

  MiSTer-Noodles renderer. Protocol 1.6 batches opaque fills and handles blended fills,
  unscaled copies, modulation, and blending. SDL's software routines provide
  coherent fallbacks for the remaining drawing operations.
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
#include <unistd.h>

#include <noodles_link.h>
#include <noodles_surface.h>

#define NOODLES_NATIVE_FORMAT SDL_PIXELFORMAT_ABGR8888
#define NOODLES_REQUIRED_PROTOCOL 0x00010004u
#define NOODLES_DEFAULT_RESIDENT_MB 192u
#define NOODLES_MAX_RESIDENT_MB 220u
#define NOODLES_DRAW_BATCH_MAX 64u
#define NOODLES_FILL_BATCH_MAX 64u
#define NOODLES_SIZE_BUCKETS 5u

typedef enum NOODLES_AlphaState
{
    NOODLES_ALPHA_UNKNOWN,
    NOODLES_ALPHA_ZERO,
    NOODLES_ALPHA_OPAQUE
} NOODLES_AlphaState;

enum
{
    NOODLES_DRAW_CLASS_COPY64,
    NOODLES_DRAW_CLASS_PLAIN_REROUTE,
    NOODLES_DRAW_CLASS_BLEND,
    NOODLES_DRAW_CLASS_REPLACE_MOD,
    NOODLES_DRAW_CLASS_ADD,
    NOODLES_DRAW_CLASS_MOD,
    NOODLES_DRAW_CLASS_MUL,
    NOODLES_DRAW_CLASS_CUSTOM,
    NOODLES_DRAW_CLASSES
};

typedef struct NOODLES_TextureData
{
    noodles_surface_t *surface;
    SDL_Surface *shadow;
    int width;
    int height;
    size_t shadow_bytes;
    SDL_bool cpu_valid;
    SDL_bool gpu_valid;
    SDL_bool keep_shadow;
    SDL_Rect lock_rect;
    SDL_bool locked;
    SDL_Rect dirty_rect;
    SDL_bool dirty_valid;
    SDL_bool pinned;
    SDL_bool gpu_in_use;
    NOODLES_AlphaState alpha_state;
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
    size_t shadow_bytes;
    size_t shadow_peak_bytes;
    Uint64 use_clock;
    noodles_surface_t *pending_draw_target;
    noodles_surface_draw_t pending_draws[NOODLES_DRAW_BATCH_MAX];
    size_t pending_draw_count;
    NOODLES_TextureData *pending_fill_target;
    noodles_surface_fill_t pending_fills[NOODLES_FILL_BATCH_MAX];
    size_t pending_fill_count;
    SDL_bool fill_batch_enabled;
    SDL_bool present_pending;
    noodles_fence_t present_fence;
    noodles_fence_t present_draw_fence;
    SDL_bool stats_enabled;
    SDL_bool in_command_queue;
    Uint64 stats_start;
    Uint64 stats_frames;
    Uint64 stats_queue_ticks;
    Uint64 stats_queue_max_ticks;
    Uint64 stats_present_ticks;
    Uint64 stats_present_max_ticks;
    Uint64 stats_fill_ticks;
    Uint64 stats_draw_ticks;
    Uint64 stats_sync_ticks;
    Uint64 stats_queue_sync_ticks;
    Uint64 stats_present_copy_ticks;
    Uint64 stats_present_wait_ticks;
    Uint64 stats_present_wait_max_ticks;
    Uint64 stats_present_waits;
    Uint64 stats_present_draw_wait_ticks;
    Uint64 stats_present_flip_wait_ticks;
    Uint64 stats_present_flip_wait_max_ticks;
    Uint64 stats_present_drawn_early;
    Uint64 stats_thread_user;
    Uint64 stats_thread_system;
    Uint64 stats_thread_voluntary;
    Uint64 stats_thread_involuntary;
    Uint64 stats_fill_commands;
    Uint64 stats_fill_pixels;
    Uint64 stats_fill_stalls;
    Uint64 stats_fill_batches;
    Uint64 stats_fill_batch_max;
    Uint64 stats_blend_fill_commands;
    Uint64 stats_blend_fill_pixels;
    Uint64 stats_blend_fill_stalls;
    Uint64 stats_plain_draws;
    Uint64 stats_plain_draw_pixels;
    Uint64 stats_flagged_draws;
    Uint64 stats_flagged_draw_pixels;
    Uint64 stats_draw_class_commands[NOODLES_DRAW_CLASSES];
    Uint64 stats_draw_class_pixels[NOODLES_DRAW_CLASSES];
    Uint64 stats_draw_size_commands[NOODLES_SIZE_BUCKETS];
    Uint64 stats_draw_size_pixels[NOODLES_SIZE_BUCKETS];
    Uint64 stats_fill_size_commands[NOODLES_SIZE_BUCKETS];
    Uint64 stats_fill_size_pixels[NOODLES_SIZE_BUCKETS];
    Uint64 stats_mirrored_draws;
    Uint64 stats_mirrored_draw_pixels;
    Uint64 stats_modulated_draws;
    Uint64 stats_modulated_draw_pixels;
    Uint64 stats_alpha_sample_draws;
    Uint64 stats_alpha_sample_pixels;
    Uint64 stats_alpha_pair_opaque;
    Uint64 stats_alpha_pair_zero;
    Uint64 stats_alpha_pair_mixed;
    Uint64 stats_alpha_unavailable_draws;
    Uint64 stats_alpha_unavailable_pixels;
    Uint64 stats_alpha_skipped_draws;
    Uint64 stats_alpha_skipped_pixels;
    Uint64 stats_alpha_copy_draws;
    Uint64 stats_alpha_copy_pixels;
    Uint64 stats_draw_stalls;
    Uint64 stats_draw_batches;
    Uint64 stats_draw_batch_max;
    Uint64 stats_software_copies;
    Uint64 stats_software_copy_pixels;
    Uint64 stats_software_fills;
    Uint64 stats_software_fill_pixels;
    Uint64 stats_primitive_commands;
    Uint64 stats_primitive_vertices;
    Uint64 stats_geometry_commands;
    Uint64 stats_geometry_triangles;
    Uint64 stats_uploads;
    Uint64 stats_upload_bytes;
    Uint64 stats_readbacks;
    Uint64 stats_readback_bytes;
    Uint64 stats_evictions;
    Uint64 stats_drains;
    Uint64 stats_progress_waits;
    Uint64 stats_shadow_allocations;
    Uint64 stats_shadow_releases;
    Uint64 stats_build_ticks;
    Uint64 stats_build_calls;
    Uint64 stats_build_state_calls;
    Uint64 stats_build_primitive_calls;
    Uint64 stats_build_fill_calls;
    Uint64 stats_build_fill_merges;
    Uint64 stats_build_copy_calls;
    Uint64 stats_build_copy_ex_calls;
    Uint64 stats_build_geometry_calls;
    Uint64 stats_create_ticks;
    Uint64 stats_create_calls;
    Uint64 stats_update_ticks;
    Uint64 stats_update_calls;
    Uint64 stats_update_bytes;
    Uint64 stats_update_deferred;
    Uint64 stats_update_rotated;
    Uint64 stats_lock_ticks;
    Uint64 stats_lock_calls;
    Uint64 stats_lock_bytes;
    Uint64 stats_unlock_ticks;
    Uint64 stats_unlock_calls;
    Uint64 stats_target_ticks;
    Uint64 stats_target_calls;
    Uint64 stats_read_pixels_ticks;
    Uint64 stats_read_pixels_calls;
    Uint64 stats_read_pixels_bytes;
    Uint64 stats_destroy_ticks;
    Uint64 stats_destroy_calls;
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

static NOODLES_AlphaState NOODLES_AlphaFromByte(Uint8 alpha)
{
    return alpha == 0 ? NOODLES_ALPHA_ZERO :
           alpha == 255 ? NOODLES_ALPHA_OPAQUE : NOODLES_ALPHA_UNKNOWN;
}

static SDL_bool NOODLES_FullRect(const NOODLES_TextureData *surface,
                                 const SDL_Rect *rect)
{
    return rect->x == 0 && rect->y == 0 &&
           rect->w == surface->width && rect->h == surface->height;
}

static NOODLES_AlphaState NOODLES_ClassifyPixels(const void *pixels, int pitch,
                                                 int width, int height)
{
    SDL_bool all_zero = SDL_TRUE;
    SDL_bool all_opaque = SDL_TRUE;
    int y;
    for (y = 0; y < height; ++y) {
        const Uint8 *row = (const Uint8 *)pixels + y * pitch;
        int x;
        for (x = 0; x < width; ++x) {
            const Uint32 alpha = row[x * 4 + 3];
            all_zero = all_zero && alpha == 0;
            all_opaque = all_opaque && alpha == 255;
            if (!all_zero && !all_opaque) {
                return NOODLES_ALPHA_UNKNOWN;
            }
        }
    }
    return all_zero ? NOODLES_ALPHA_ZERO :
           all_opaque ? NOODLES_ALPHA_OPAQUE : NOODLES_ALPHA_UNKNOWN;
}

static void NOODLES_UpdateAlphaPixels(NOODLES_TextureData *surface,
                                      const SDL_Rect *rect, const void *pixels,
                                      int pitch)
{
    const NOODLES_AlphaState incoming =
        NOODLES_ClassifyPixels(pixels, pitch, rect->w, rect->h);
    if (NOODLES_FullRect(surface, rect)) {
        surface->alpha_state = incoming;
    } else if (incoming == NOODLES_ALPHA_UNKNOWN ||
               surface->alpha_state != incoming) {
        surface->alpha_state = NOODLES_ALPHA_UNKNOWN;
    }
}

static NOODLES_AlphaState NOODLES_FillAlphaState(
    const NOODLES_TextureData *target, NOODLES_AlphaState current,
    const SDL_Rect *rect, Uint8 alpha, SDL_BlendMode blend)
{
    const NOODLES_AlphaState source = NOODLES_AlphaFromByte(alpha);
    const SDL_bool full = NOODLES_FullRect(target, rect);
    if (blend == SDL_BLENDMODE_NONE) {
        if (full) {
            return source;
        }
        return current == source ? current : NOODLES_ALPHA_UNKNOWN;
    }
    if (blend == SDL_BLENDMODE_BLEND) {
        if (current == NOODLES_ALPHA_OPAQUE || source == NOODLES_ALPHA_ZERO) {
            return current;
        }
        if (full && source == NOODLES_ALPHA_OPAQUE) {
            return NOODLES_ALPHA_OPAQUE;
        }
        return NOODLES_ALPHA_UNKNOWN;
    }
    if (blend == SDL_BLENDMODE_ADD || blend == SDL_BLENDMODE_MOD ||
        blend == SDL_BLENDMODE_MUL) {
        return current;
    }
    return NOODLES_ALPHA_UNKNOWN;
}

static NOODLES_AlphaState NOODLES_CopyAlphaState(
    const NOODLES_TextureData *target, const SDL_Rect *destination,
    NOODLES_AlphaState source, Uint8 alpha_mod, SDL_BlendMode blend)
{
    const SDL_bool full = NOODLES_FullRect(target, destination);
    if (blend == SDL_BLENDMODE_NONE) {
        const NOODLES_AlphaState output =
            source == NOODLES_ALPHA_ZERO ? NOODLES_ALPHA_ZERO :
            source == NOODLES_ALPHA_OPAQUE && alpha_mod == 255
                ? NOODLES_ALPHA_OPAQUE : NOODLES_ALPHA_UNKNOWN;
        if (full) {
            return output;
        }
        return target->alpha_state == output ? output : NOODLES_ALPHA_UNKNOWN;
    }
    if (blend == SDL_BLENDMODE_BLEND) {
        if (source == NOODLES_ALPHA_ZERO) {
            return target->alpha_state;
        }
        if (target->alpha_state == NOODLES_ALPHA_OPAQUE ||
            (full && source == NOODLES_ALPHA_OPAQUE && alpha_mod == 255)) {
            return NOODLES_ALPHA_OPAQUE;
        }
        return NOODLES_ALPHA_UNKNOWN;
    }
    if (blend == SDL_BLENDMODE_ADD || blend == SDL_BLENDMODE_MOD ||
        blend == SDL_BLENDMODE_MUL) {
        return target->alpha_state;
    }
    return NOODLES_ALPHA_UNKNOWN;
}

static int NOODLES_FlushFills(NOODLES_RenderData *data);
static int NOODLES_FlushDraws(NOODLES_RenderData *data);
static int NOODLES_FlushQueued(NOODLES_RenderData *data);

static int NOODLES_DrainLink(NOODLES_RenderData *data)
{
    if (data->stats_enabled) {
        data->stats_drains++;
    }
    if (noodles_link_drain(data->link, NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
        return NOODLES_SetErrno("drain");
    }
    return 0;
}

static int NOODLES_WaitProgress(NOODLES_RenderData *data)
{
    if (data->stats_enabled) {
        data->stats_progress_waits++;
    }
    if (noodles_link_wait_progress(data->link, NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
        return NOODLES_SetErrno("queue progress");
    }
    return 0;
}

static void NOODLES_AddTiming(NOODLES_RenderData *data, Uint64 start,
                              Uint64 *total)
{
    if (data->stats_enabled) {
        *total += SDL_GetPerformanceCounter() - start;
    }
}

/* With statistics enabled, first waits for the commands queued before the
   pending PRESENT so the deferred wait separates remaining draw work from
   the flip, retirement and vertical-blank interval. */
static int NOODLES_WaitPresent(NOODLES_RenderData *data)
{
    Uint64 start;
    Uint64 drawn;
    Uint64 elapsed;
    int complete = 0;
    if (!data->present_pending) {
        return 0;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    drawn = start;
    if (data->stats_enabled) {
        if (noodles_link_poll(data->link, data->present_draw_fence, &complete) < 0) {
            return NOODLES_SetErrno("present draw poll");
        }
        if (complete) {
            data->stats_present_drawn_early++;
        } else {
            if (noodles_link_wait(data->link, data->present_draw_fence,
                                  NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
                return NOODLES_SetErrno("present draw completion");
            }
            drawn = SDL_GetPerformanceCounter();
            data->stats_present_draw_wait_ticks += drawn - start;
        }
    }
    if (noodles_link_wait(data->link, data->present_fence,
                          NOODLES_DEFAULT_TIMEOUT_MS) < 0) {
        return NOODLES_SetErrno("present completion");
    }
    data->present_pending = SDL_FALSE;
    if (data->stats_enabled) {
        const Uint64 now = SDL_GetPerformanceCounter();
        elapsed = now - start;
        data->stats_present_wait_ticks += elapsed;
        data->stats_present_wait_max_ticks = SDL_max(
            data->stats_present_wait_max_ticks, elapsed);
        data->stats_present_waits++;
        data->stats_present_flip_wait_ticks += now - drawn;
        data->stats_present_flip_wait_max_ticks = SDL_max(
            data->stats_present_flip_wait_max_ticks, now - drawn);
    }
    return 0;
}

/* Reads the calling thread's cumulative user and system clock ticks and
   voluntary and involuntary context switches. Returns zero on success. */
static int NOODLES_ReadThreadUsage(Uint64 *user, Uint64 *system,
                                   Uint64 *voluntary, Uint64 *involuntary)
{
    char buffer[2048];
    const char *field;
    size_t length;
    int i;
    SDL_RWops *file = SDL_RWFromFile("/proc/thread-self/stat", "r");
    if (!file) {
        return -1;
    }
    length = SDL_RWread(file, buffer, 1, sizeof(buffer) - 1);
    SDL_RWclose(file);
    buffer[length] = '\0';
    /* Fields after the parenthesized command name start with state (3);
       utime and stime are fields 14 and 15. */
    field = SDL_strrchr(buffer, ')');
    if (!field) {
        return -1;
    }
    for (i = 3; i < 14 && field; ++i) {
        field = SDL_strchr(field + 1, ' ');
    }
    if (!field) {
        return -1;
    }
    *user = SDL_strtoull(field + 1, (char **)&field, 10);
    *system = SDL_strtoull(field, NULL, 10);

    file = SDL_RWFromFile("/proc/thread-self/status", "r");
    if (!file) {
        return -1;
    }
    length = SDL_RWread(file, buffer, 1, sizeof(buffer) - 1);
    SDL_RWclose(file);
    buffer[length] = '\0';
    field = SDL_strstr(buffer, "voluntary_ctxt_switches:");
    if (!field) {
        return -1;
    }
    *voluntary = SDL_strtoull(field + 24, NULL, 10);
    field = SDL_strstr(field + 24, "nonvoluntary_ctxt_switches:");
    if (!field) {
        return -1;
    }
    *involuntary = SDL_strtoull(field + 27, NULL, 10);
    return 0;
}

static int NOODLES_SurfaceRead(NOODLES_RenderData *data,
                               noodles_surface_t *surface,
                               const noodles_rect_t *rect, void *pixels,
                               size_t pitch)
{
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    int result;
    if (surface) {
        result = noodles_surface_read(surface, rect, pixels, pitch,
                                      NOODLES_DEFAULT_TIMEOUT_MS);
    } else if (NOODLES_WaitPresent(data) < 0) {
        result = -1;
    } else {
        result = noodles_back_buffer_read(data->link, rect, pixels, pitch,
                                          NOODLES_DEFAULT_TIMEOUT_MS);
    }
    if (data->stats_enabled) {
        const Uint64 elapsed = SDL_GetPerformanceCounter() - start;
        data->stats_sync_ticks += elapsed;
        if (data->in_command_queue) {
            data->stats_queue_sync_ticks += elapsed;
        }
    }
    return result;
}

static int NOODLES_SurfaceUpdate(NOODLES_RenderData *data,
                                 noodles_surface_t *surface,
                                 const noodles_rect_t *rect,
                                 const void *pixels, size_t pitch)
{
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    int result;
    if (surface) {
        result = noodles_surface_update(surface, rect, pixels, pitch,
                                        NOODLES_DEFAULT_TIMEOUT_MS);
    } else if (NOODLES_WaitPresent(data) < 0) {
        result = -1;
    } else {
        result = noodles_back_buffer_update(data->link, rect, pixels, pitch,
                                            NOODLES_DEFAULT_TIMEOUT_MS);
    }
    if (data->stats_enabled) {
        const Uint64 elapsed = SDL_GetPerformanceCounter() - start;
        data->stats_sync_ticks += elapsed;
        if (data->in_command_queue) {
            data->stats_queue_sync_ticks += elapsed;
        }
    }
    return result;
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

static int NOODLES_AllocateShadow(NOODLES_RenderData *data,
                                  NOODLES_TextureData *surface)
{
    if (surface->shadow) {
        return 0;
    }
    surface->shadow = SDL_CreateRGBSurfaceWithFormat(0, surface->width,
                                                     surface->height, 32,
                                                     NOODLES_NATIVE_FORMAT);
    if (!surface->shadow) {
        return -1;
    }
    surface->shadow_bytes = (size_t)surface->shadow->pitch *
                            (size_t)surface->shadow->h;
    data->shadow_bytes += surface->shadow_bytes;
    data->shadow_peak_bytes = SDL_max(data->shadow_peak_bytes,
                                      data->shadow_bytes);
    if (data->stats_enabled) {
        data->stats_shadow_allocations++;
    }
    return 0;
}

static void NOODLES_FreeShadow(NOODLES_RenderData *data,
                               NOODLES_TextureData *surface)
{
    if (!surface->shadow) {
        return;
    }
    data->shadow_bytes -= surface->shadow_bytes;
    SDL_FreeSurface(surface->shadow);
    surface->shadow = NULL;
    surface->shadow_bytes = 0;
    surface->cpu_valid = SDL_FALSE;
    if (data->stats_enabled) {
        data->stats_shadow_releases++;
    }
}

static void NOODLES_DropRedundantShadow(NOODLES_RenderData *data,
                                        NOODLES_TextureData *surface)
{
    if (!surface->pinned && !surface->keep_shadow &&
        surface != data->target && !surface->locked &&
        surface->surface && surface->gpu_valid) {
        NOODLES_FreeShadow(data, surface);
    }
}

static void NOODLES_ClearDirty(NOODLES_TextureData *surface)
{
    surface->dirty_valid = SDL_FALSE;
}

static void NOODLES_MarkDirty(NOODLES_TextureData *surface,
                              const SDL_Rect *rect)
{
    if (surface->dirty_valid) {
        SDL_UnionRect(&surface->dirty_rect, rect, &surface->dirty_rect);
    } else {
        surface->dirty_rect = *rect;
        surface->dirty_valid = SDL_TRUE;
    }
}

static void NOODLES_MarkAllDirty(NOODLES_TextureData *surface)
{
    SDL_Rect rect = { 0, 0, surface->width, surface->height };
    NOODLES_MarkDirty(surface, &rect);
}

static int NOODLES_InitSurface(NOODLES_RenderData *data, int width, int height,
                               SDL_bool pinned, NOODLES_TextureData *surface)
{
    SDL_zerop(surface);
    surface->width = width;
    surface->height = height;
    if (NOODLES_AllocateShadow(data, surface) < 0) {
        return -1;
    }
    SDL_memset(surface->shadow->pixels, 0,
               (size_t)surface->shadow->pitch * (size_t)surface->shadow->h);
    surface->cpu_valid = SDL_TRUE;
    surface->alpha_state = NOODLES_ALPHA_ZERO;
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
    (void)NOODLES_FlushQueued(data);
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
    NOODLES_FreeShadow(data, surface);
    SDL_zerop(surface);
}

static int NOODLES_EnsureCPU(NOODLES_RenderData *data, NOODLES_TextureData *surface)
{
    noodles_rect_t rect;
    if (NOODLES_FlushQueued(data) < 0) {
        return -1;
    }
    if (surface->cpu_valid && surface->shadow) {
        return 0;
    }
    surface->cpu_valid = SDL_FALSE;
    if (NOODLES_AllocateShadow(data, surface) < 0) {
        return -1;
    }
    if (!surface->gpu_valid) {
        SDL_memset(surface->shadow->pixels, 0,
                   (size_t)surface->shadow->pitch * (size_t)surface->shadow->h);
        surface->cpu_valid = SDL_TRUE;
        surface->alpha_state = NOODLES_ALPHA_ZERO;
        return 0;
    }
    if (!surface->surface && surface != &data->composition) {
        return SDL_SetError("Noodles evicted surface has no CPU contents");
    }
    rect.x = 0;
    rect.y = 0;
    rect.width = (Uint32)surface->width;
    rect.height = (Uint32)surface->height;
    if (NOODLES_SurfaceRead(data, surface->surface, &rect,
                            surface->shadow->pixels,
                            (size_t)surface->shadow->pitch) < 0) {
        return NOODLES_SetErrno("fallback readback");
    }
    if (data->stats_enabled) {
        data->stats_readbacks++;
        data->stats_readback_bytes += (Uint64)rect.width * rect.height * 4u;
    }
    surface->cpu_valid = SDL_TRUE;
    surface->gpu_in_use = SDL_FALSE;
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
    if (data->stats_enabled) {
        data->stats_evictions++;
    }
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
    if (surface == &data->composition) {
        NOODLES_TouchSurface(data, surface);
        return 0;
    }
    if (surface->surface) {
        NOODLES_TouchSurface(data, surface);
        return 0;
    }
    if (NOODLES_FlushQueued(data) < 0) {
        return -1;
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
        if (noodles_surface_create(data->link, (Uint32)surface->width,
                                   (Uint32)surface->height,
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
    const void *pixels;
    const SDL_bool partial = surface->surface && surface->dirty_valid;
    if ((((surface != &data->composition) && !surface->surface) ||
         !surface->gpu_valid) &&
        NOODLES_FlushQueued(data) < 0) {
        return -1;
    }
    if (NOODLES_MakeResident(data, surface) < 0) {
        return -1;
    }
    if (surface->gpu_valid) {
        NOODLES_DropRedundantShadow(data, surface);
        return 0;
    }
    if (!surface->cpu_valid) {
        return SDL_SetError("Noodles surface has no current contents");
    }
    if (partial) {
        rect.x = surface->dirty_rect.x;
        rect.y = surface->dirty_rect.y;
        rect.width = (Uint32)surface->dirty_rect.w;
        rect.height = (Uint32)surface->dirty_rect.h;
        pixels = (const Uint8 *)surface->shadow->pixels +
                 surface->dirty_rect.y * surface->shadow->pitch +
                 surface->dirty_rect.x * 4;
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.width = (Uint32)surface->width;
        rect.height = (Uint32)surface->height;
        pixels = surface->shadow->pixels;
    }
    if (NOODLES_SurfaceUpdate(data, surface->surface, &rect,
                              pixels,
                              (size_t)surface->shadow->pitch) < 0) {
        return NOODLES_SetErrno("fallback upload");
    }
    if (data->stats_enabled) {
        data->stats_uploads++;
        data->stats_upload_bytes += (Uint64)rect.width * rect.height * 4u;
    }
    surface->gpu_valid = SDL_TRUE;
    surface->gpu_in_use = SDL_FALSE;
    NOODLES_ClearDirty(surface);
    NOODLES_DropRedundantShadow(data, surface);
    return 0;
}

/* Replace a texture still referenced by queued GPU work with a fresh managed
   surface. The old allocation remains alive in the SDK until its last-use
   fence retires, while the new full image is ready for commands after the
   pending presentation. Return one when rotated, zero when allocation pressure
   requires the ordinary deferred upload path, and minus one on a hard error. */
static int NOODLES_TryRotateTexture(NOODLES_RenderData *data,
                                    NOODLES_TextureData *texture)
{
    noodles_surface_t *replacement;
    noodles_surface_t *old_surface;
    noodles_rect_t rect;

    if (!data->present_pending || !texture->surface ||
        !texture->gpu_in_use || !texture->cpu_valid || !texture->shadow) {
        return 0;
    }
    if (NOODLES_FlushQueued(data) < 0) {
        return -1;
    }
    if (noodles_surface_create(data->link, (Uint32)texture->width,
                               (Uint32)texture->height, &replacement) < 0) {
        if (errno == ENOMEM || errno == EAGAIN) {
            return 0;
        }
        return NOODLES_SetErrno("texture rotation allocation");
    }
    rect.x = 0;
    rect.y = 0;
    rect.width = (Uint32)texture->width;
    rect.height = (Uint32)texture->height;
    if (NOODLES_SurfaceUpdate(data, replacement, &rect,
                              texture->shadow->pixels,
                              (size_t)texture->shadow->pitch) < 0) {
        const int saved_errno = errno;
        (void)noodles_surface_destroy(replacement);
        errno = saved_errno;
        return NOODLES_SetErrno("texture rotation upload");
    }
    old_surface = texture->surface;
    if (noodles_surface_destroy(old_surface) < 0) {
        const int saved_errno = errno;
        (void)noodles_surface_destroy(replacement);
        errno = saved_errno;
        return NOODLES_SetErrno("texture rotation retirement");
    }
    texture->surface = replacement;
    texture->gpu_valid = SDL_TRUE;
    texture->gpu_in_use = SDL_FALSE;
    NOODLES_ClearDirty(texture);
    NOODLES_TouchSurface(data, texture);
    if (data->stats_enabled) {
        data->stats_uploads++;
        data->stats_upload_bytes += (Uint64)rect.width * rect.height * 4u;
        data->stats_update_rotated++;
    }
    return 1;
}

static int NOODLES_PrepareCPURegion(NOODLES_RenderData *data,
                                    NOODLES_TextureData *surface,
                                    const SDL_Rect *region)
{
    noodles_rect_t rect;
    void *pixels;
    if (NOODLES_FlushQueued(data) < 0) {
        return -1;
    }
    if (NOODLES_EnsureGPU(data, surface) < 0) {
        return -1;
    }
    if (NOODLES_AllocateShadow(data, surface) < 0) {
        return -1;
    }
    if (surface->cpu_valid) {
        return 0;
    }
    rect.x = region->x;
    rect.y = region->y;
    rect.width = (Uint32)region->w;
    rect.height = (Uint32)region->h;
    pixels = (Uint8 *)surface->shadow->pixels +
             region->y * surface->shadow->pitch + region->x * 4;
    if (NOODLES_SurfaceRead(data, surface->surface, &rect, pixels,
                            (size_t)surface->shadow->pitch) < 0) {
        return NOODLES_SetErrno("regional readback");
    }
    if (data->stats_enabled) {
        data->stats_readbacks++;
        data->stats_readback_bytes += (Uint64)rect.width * rect.height * 4u;
    }
    surface->gpu_in_use = SDL_FALSE;
    NOODLES_TouchSurface(data, surface);
    return 0;
}

static int NOODLES_CommitCPURegion(NOODLES_RenderData *data,
                                   NOODLES_TextureData *surface,
                                   const SDL_Rect *region)
{
    noodles_rect_t rect;
    const void *pixels;
    rect.x = region->x;
    rect.y = region->y;
    rect.width = (Uint32)region->w;
    rect.height = (Uint32)region->h;
    pixels = (const Uint8 *)surface->shadow->pixels +
             region->y * surface->shadow->pitch + region->x * 4;
    if (NOODLES_SurfaceUpdate(data, surface->surface, &rect, pixels,
                              (size_t)surface->shadow->pitch) < 0) {
        return NOODLES_SetErrno("regional upload");
    }
    if (data->stats_enabled) {
        data->stats_uploads++;
        data->stats_upload_bytes += (Uint64)rect.width * rect.height * 4u;
    }
    surface->gpu_valid = SDL_TRUE;
    surface->gpu_in_use = SDL_FALSE;
    surface->alpha_state = NOODLES_ALPHA_UNKNOWN;
    NOODLES_ClearDirty(surface);
    NOODLES_TouchSurface(data, surface);
    return 0;
}

static void NOODLES_MarkGPUWriteState(NOODLES_TextureData *surface,
                                      NOODLES_AlphaState alpha_state)
{
    surface->gpu_valid = SDL_TRUE;
    surface->cpu_valid = SDL_FALSE;
    surface->gpu_in_use = SDL_TRUE;
    surface->alpha_state = alpha_state;
    NOODLES_ClearDirty(surface);
}

static void NOODLES_MarkCPUWrite(NOODLES_TextureData *surface)
{
    surface->cpu_valid = SDL_TRUE;
    surface->gpu_valid = SDL_FALSE;
    surface->alpha_state = NOODLES_ALPHA_UNKNOWN;
    NOODLES_MarkAllDirty(surface);
}

static int NOODLES_FlushFills(NOODLES_RenderData *data)
{
    size_t i;
    const size_t count = data->pending_fill_count;
    Uint64 start;

    if (!count) {
        return 0;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    for (;;) {
        if (noodles_surface_fill_batch(data->link,
                data->pending_fill_target == &data->composition
                    ? NULL : data->pending_fill_target->surface,
                data->pending_fills, count) == 0) {
            break;
        }
        if (errno != EAGAIN) {
            NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
            return NOODLES_SetErrno("fill batch");
        }
        if (data->stats_enabled) {
            data->stats_fill_stalls++;
        }
        if (NOODLES_WaitProgress(data) < 0) {
            NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
            return -1;
        }
    }
    NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
    if (data->stats_enabled) {
        data->stats_fill_commands += count;
        data->stats_fill_batches++;
        data->stats_fill_batch_max = SDL_max(data->stats_fill_batch_max,
                                             (Uint64)count);
        for (i = 0; i < count; ++i) {
            const Uint64 pixels = (Uint64)data->pending_fills[i].rect.width *
                                  data->pending_fills[i].rect.height;
            const unsigned bucket = pixels <= 64u ? 0u : pixels <= 1024u ? 1u :
                                    pixels <= 4096u ? 2u : pixels <= 16384u ? 3u : 4u;
            data->stats_fill_pixels += pixels;
            data->stats_fill_size_commands[bucket]++;
            data->stats_fill_size_pixels[bucket] += pixels;
        }
    }
    data->pending_fill_count = 0;
    data->pending_fill_target = NULL;
    return 0;
}

static int NOODLES_SubmitFill(NOODLES_RenderData *data, NOODLES_TextureData *target,
                              const noodles_rect_t *rect, Uint32 color)
{
    Uint64 start;
    if (NOODLES_FlushDraws(data) < 0) {
        return -1;
    }
    if (data->fill_batch_enabled) {
        if (data->pending_fill_count && data->pending_fill_target != target &&
            NOODLES_FlushFills(data) < 0) {
            return -1;
        }
        if (!data->pending_fill_count) {
            data->pending_fill_target = target;
        }
        data->pending_fills[data->pending_fill_count].rect = *rect;
        data->pending_fills[data->pending_fill_count].color = color;
        data->pending_fill_count++;
        if (data->pending_fill_count == NOODLES_FILL_BATCH_MAX) {
            return NOODLES_FlushFills(data);
        }
        return 0;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    for (;;) {
        if ((target == &data->composition
                ? noodles_back_buffer_fill(data->link, rect, color)
                : noodles_surface_fill(target->surface, rect, color)) == 0) {
            break;
        }
        if (errno != EAGAIN) {
            NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
            return NOODLES_SetErrno("fill");
        }
        if (data->stats_enabled) {
            data->stats_fill_stalls++;
        }
        if (NOODLES_WaitProgress(data) < 0) {
            NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
            return -1;
        }
    }
    if (data->stats_enabled) {
        const Uint64 pixels = (Uint64)rect->width * rect->height;
        const unsigned bucket = pixels <= 64u ? 0u : pixels <= 1024u ? 1u :
                                pixels <= 4096u ? 2u : pixels <= 16384u ? 3u : 4u;
        data->stats_fill_commands++;
        data->stats_fill_pixels += pixels;
        data->stats_fill_size_commands[bucket]++;
        data->stats_fill_size_pixels[bucket] += pixels;
    }
    NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
    return 0;
}

static int NOODLES_SubmitBlendFill(NOODLES_RenderData *data,
                                   NOODLES_TextureData *target,
                                   const noodles_rect_t *rect, Uint32 color,
                                   Uint32 blend_mode)
{
    Uint64 start;
    if (NOODLES_FlushQueued(data) < 0) {
        return -1;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    for (;;) {
        if ((target == &data->composition
                ? noodles_back_buffer_blend_fill(data->link, rect, color, blend_mode)
                : noodles_surface_blend_fill(target->surface, rect, color,
                                             blend_mode)) == 0) {
            break;
        }
        if (errno != EAGAIN) {
            NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
            return NOODLES_SetErrno("blended fill");
        }
        if (data->stats_enabled) {
            data->stats_blend_fill_stalls++;
        }
        if (NOODLES_WaitProgress(data) < 0) {
            NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
            return -1;
        }
    }
    if (data->stats_enabled) {
        const Uint64 pixels = (Uint64)rect->width * rect->height;
        const unsigned bucket = pixels <= 64u ? 0u : pixels <= 1024u ? 1u :
                                pixels <= 4096u ? 2u : pixels <= 16384u ? 3u : 4u;
        data->stats_blend_fill_commands++;
        data->stats_blend_fill_pixels += pixels;
        data->stats_fill_size_commands[bucket]++;
        data->stats_fill_size_pixels[bucket] += pixels;
    }
    NOODLES_AddTiming(data, start, &data->stats_fill_ticks);
    return 0;
}

static int NOODLES_FlushDraws(NOODLES_RenderData *data)
{
    size_t i;
    const size_t count = data->pending_draw_count;
    Uint64 start;

    if (!count) {
        return 0;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    for (;;) {
        if (noodles_surface_draw_batch(data->link, data->pending_draw_target,
                                       data->pending_draws, count) == 0) {
            break;
        }
        if (errno != EAGAIN) {
            NOODLES_AddTiming(data, start, &data->stats_draw_ticks);
            return NOODLES_SetErrno("draw batch");
        }
        if (data->stats_enabled) {
            data->stats_draw_stalls++;
        }
        if (NOODLES_WaitProgress(data) < 0) {
            NOODLES_AddTiming(data, start, &data->stats_draw_ticks);
            return -1;
        }
    }
    NOODLES_AddTiming(data, start, &data->stats_draw_ticks);
    if (data->stats_enabled) {
        data->stats_draw_batches++;
        data->stats_draw_batch_max = SDL_max(data->stats_draw_batch_max,
                                             (Uint64)count);
        for (i = 0; i < count; ++i) {
            const noodles_surface_draw_t *draw = &data->pending_draws[i];
            const Uint64 pixels = (Uint64)draw->source_rect.width *
                                  draw->source_rect.height;
            const Uint32 mode = draw->flags & ~(NOODLES_DRAW_MIRROR_X |
                                                 NOODLES_DRAW_MIRROR_Y);
            const Uint32 pitch = noodles_surface_pitch(draw->source);
            const Uint32 source_offset = (Uint32)draw->source_rect.y * pitch +
                                         (Uint32)draw->source_rect.x * 4u;
            const SDL_bool copy64_safe = !(draw->source_rect.width & 1u) &&
                                         !(pitch & 7u) && !(source_offset & 7u);
            const unsigned size_bucket = pixels <= 64u ? 0u : pixels <= 1024u ? 1u :
                                         pixels <= 4096u ? 2u :
                                         pixels <= 16384u ? 3u : 4u;
            unsigned draw_class;
            if (draw->flags) {
                data->stats_flagged_draws++;
                data->stats_flagged_draw_pixels += pixels;
            } else {
                data->stats_plain_draws++;
                data->stats_plain_draw_pixels += pixels;
            }
            if (mode == 0u) {
                draw_class = copy64_safe ? NOODLES_DRAW_CLASS_COPY64 :
                                           NOODLES_DRAW_CLASS_PLAIN_REROUTE;
            } else if (mode == NOODLES_DRAW_BLEND || mode == NOODLES_DRAW_MODE_BLEND) {
                draw_class = NOODLES_DRAW_CLASS_BLEND;
            } else if (mode == NOODLES_DRAW_MODE_NONE) {
                draw_class = NOODLES_DRAW_CLASS_REPLACE_MOD;
            } else if (mode == NOODLES_DRAW_MODE_ADD) {
                draw_class = NOODLES_DRAW_CLASS_ADD;
            } else if (mode == NOODLES_DRAW_MODE_MOD) {
                draw_class = NOODLES_DRAW_CLASS_MOD;
            } else if (mode == NOODLES_DRAW_MODE_MUL) {
                draw_class = NOODLES_DRAW_CLASS_MUL;
            } else {
                draw_class = NOODLES_DRAW_CLASS_CUSTOM;
            }
            data->stats_draw_class_commands[draw_class]++;
            data->stats_draw_class_pixels[draw_class] += pixels;
            data->stats_draw_size_commands[size_bucket]++;
            data->stats_draw_size_pixels[size_bucket] += pixels;
            if (draw->flags & (NOODLES_DRAW_MIRROR_X | NOODLES_DRAW_MIRROR_Y)) {
                data->stats_mirrored_draws++;
                data->stats_mirrored_draw_pixels += pixels;
            }
            if (draw->modulation != 0xffffffffu) {
                data->stats_modulated_draws++;
                data->stats_modulated_draw_pixels += pixels;
            }
        }
    }
    data->pending_draw_count = 0;
    data->pending_draw_target = NULL;
    return 0;
}

static int NOODLES_FlushQueued(NOODLES_RenderData *data)
{
    if (NOODLES_FlushFills(data) < 0) {
        return -1;
    }
    return NOODLES_FlushDraws(data);
}

static int NOODLES_SubmitDraw(NOODLES_RenderData *data, noodles_surface_t *target,
                              const noodles_surface_draw_t *draw)
{
    if (NOODLES_FlushFills(data) < 0) {
        return -1;
    }
    if (data->pending_draw_count && data->pending_draw_target != target &&
        NOODLES_FlushDraws(data) < 0) {
        return -1;
    }
    if (!data->pending_draw_count) {
        data->pending_draw_target = target;
    }
    data->pending_draws[data->pending_draw_count++] = *draw;
    if (data->pending_draw_count == NOODLES_DRAW_BATCH_MAX) {
        return NOODLES_FlushDraws(data);
    }
    return 0;
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

static int NOODLES_CreateTextureImpl(SDL_Renderer *renderer, SDL_Texture *texture)
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

static int NOODLES_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_CreateTextureImpl(renderer, texture);
    if (data->stats_enabled) {
        data->stats_create_ticks += SDL_GetPerformanceCounter() - start;
        data->stats_create_calls++;
    }
    return result;
}

static int NOODLES_UpdateTextureImpl(SDL_Renderer *renderer, SDL_Texture *texture,
                                 const SDL_Rect *rect, const void *pixels, int pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    noodles_rect_t update_rect;
    Uint8 *destination;
    const Uint8 *source;
    int row;
    int rotated;
    const SDL_bool gpu_was_valid = texturedata->gpu_valid;

    texturedata->keep_shadow = SDL_TRUE;
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
    NOODLES_UpdateAlphaPixels(texturedata, rect, pixels, pitch);

    texturedata->cpu_valid = SDL_TRUE;
    texturedata->gpu_valid = SDL_FALSE;
    NOODLES_MarkDirty(texturedata, rect);
    rotated = gpu_was_valid ? NOODLES_TryRotateTexture(data, texturedata) : 0;
    if (rotated < 0) {
        return -1;
    }
    if (rotated > 0) {
        return 0;
    }
    if (texturedata->surface && gpu_was_valid &&
        (!data->present_pending || !texturedata->gpu_in_use)) {
        update_rect.x = rect->x;
        update_rect.y = rect->y;
        update_rect.width = (Uint32)rect->w;
        update_rect.height = (Uint32)rect->h;
        if (NOODLES_SurfaceUpdate(data, texturedata->surface, &update_rect,
                                  pixels, (size_t)pitch) < 0) {
            return NOODLES_SetErrno("texture update");
        }
        if (data->stats_enabled) {
            data->stats_uploads++;
            data->stats_upload_bytes += (Uint64)update_rect.width * update_rect.height * 4u;
        }
        texturedata->gpu_valid = SDL_TRUE;
        texturedata->gpu_in_use = SDL_FALSE;
        NOODLES_ClearDirty(texturedata);
        NOODLES_TouchSurface(data, texturedata);
        NOODLES_DropRedundantShadow(data, texturedata);
    } else if (data->present_pending && data->stats_enabled) {
        data->stats_update_deferred++;
    }
    return 0;
}

static int NOODLES_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                                 const SDL_Rect *rect, const void *pixels, int pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    Uint64 start;
    if ((!texturedata->shadow || !texturedata->cpu_valid) &&
        NOODLES_WaitPresent(data) < 0) {
        return -1;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_UpdateTextureImpl(renderer, texture, rect, pixels, pitch);
    if (data->stats_enabled) {
        data->stats_update_ticks += SDL_GetPerformanceCounter() - start;
        data->stats_update_calls++;
        data->stats_update_bytes += (Uint64)rect->w * rect->h * 4u;
    }
    return result;
}

static int NOODLES_LockTextureImpl(SDL_Renderer *renderer, SDL_Texture *texture,
                               const SDL_Rect *rect, void **pixels, int *pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;

    if (texturedata->locked) {
        return SDL_SetError("Noodles texture is not lockable");
    }
    texturedata->keep_shadow = SDL_TRUE;
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

static int NOODLES_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                               const SDL_Rect *rect, void **pixels, int *pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    Uint64 start;
    if ((!texturedata->shadow || !texturedata->cpu_valid) &&
        NOODLES_WaitPresent(data) < 0) {
        return -1;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_LockTextureImpl(renderer, texture, rect, pixels, pitch);
    if (data->stats_enabled) {
        data->stats_lock_ticks += SDL_GetPerformanceCounter() - start;
        data->stats_lock_calls++;
        data->stats_lock_bytes += (Uint64)rect->w * rect->h * 4u;
    }
    return result;
}

static void NOODLES_UnlockTextureImpl(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
    noodles_rect_t rect;
    const SDL_bool gpu_was_valid = texturedata->gpu_valid;
    const void *pixels;
    size_t pitch;
    int rotated;

    if (!texturedata->locked) {
        return;
    }
    texturedata->locked = SDL_FALSE;
    pixels = (const Uint8 *)texturedata->shadow->pixels +
             texturedata->lock_rect.y * texturedata->shadow->pitch +
             texturedata->lock_rect.x * 4;
    pitch = (size_t)texturedata->shadow->pitch;
    NOODLES_UpdateAlphaPixels(texturedata, &texturedata->lock_rect,
                              pixels, (int)pitch);
    texturedata->cpu_valid = SDL_TRUE;
    texturedata->gpu_valid = SDL_FALSE;
    NOODLES_MarkDirty(texturedata, &texturedata->lock_rect);
    if (!texturedata->surface) {
        return;
    }
    rotated = gpu_was_valid ? NOODLES_TryRotateTexture(data, texturedata) : 0;
    if (rotated < 0) {
        return;
    }
    if (rotated > 0) {
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
        rect.width = (Uint32)texturedata->width;
        rect.height = (Uint32)texturedata->height;
        pixels = texturedata->shadow->pixels;
        pitch = (size_t)texturedata->shadow->pitch;
    }
    if (data->present_pending && texturedata->gpu_in_use) {
        if (data->stats_enabled) {
            data->stats_update_deferred++;
        }
        return;
    }
    if (NOODLES_SurfaceUpdate(data, texturedata->surface, &rect,
                              pixels, pitch) == 0) {
        if (data->stats_enabled) {
            data->stats_uploads++;
            data->stats_upload_bytes += (Uint64)rect.width * rect.height * 4u;
        }
        texturedata->gpu_valid = SDL_TRUE;
        texturedata->gpu_in_use = SDL_FALSE;
        NOODLES_ClearDirty(texturedata);
        NOODLES_TouchSurface(data, texturedata);
        NOODLES_DropRedundantShadow(data, texturedata);
    } else {
        (void)NOODLES_SetErrno("texture unlock");
    }
}

static void NOODLES_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    Uint64 start;
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    NOODLES_UnlockTextureImpl(renderer, texture);
    if (data->stats_enabled) {
        data->stats_unlock_ticks += SDL_GetPerformanceCounter() - start;
        data->stats_unlock_calls++;
    }
}

static void NOODLES_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture,
                                        SDL_ScaleMode scaleMode)
{
    (void)renderer;
    (void)texture;
    (void)scaleMode;
}

static int NOODLES_SetRenderTargetImpl(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *old_target = data->target;
    if (NOODLES_FlushQueued(data) < 0) {
        return -1;
    }
    if (texture) {
        NOODLES_TextureData *texturedata = (NOODLES_TextureData *)texture->driverdata;
        data->target = texturedata;
    } else {
        data->target = &data->composition;
    }
    if (old_target != data->target) {
        NOODLES_DropRedundantShadow(data, old_target);
    }
    return 0;
}

static int NOODLES_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_SetRenderTargetImpl(renderer, texture);
    if (data->stats_enabled) {
        data->stats_target_ticks += SDL_GetPerformanceCounter() - start;
        data->stats_target_calls++;
    }
    return result;
}

static int NOODLES_QueueNoOpImpl(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    (void)renderer;
    (void)cmd;
    return 0;
}

static int NOODLES_QueueDrawPointsImpl(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
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

static int NOODLES_QueueFillRectsImpl(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                  const SDL_FRect *rects, int count)
{
    const size_t old_count = cmd->data.draw.count;
    size_t first;
    SDL_Rect *queued = (SDL_Rect *)SDL_AllocateRenderVertices(renderer,
                                                              (size_t)count * sizeof(*queued),
                                                              0, &first);
    int i;
    if (!queued) {
        return -1;
    }
    if (old_count && first != cmd->data.draw.first + old_count * sizeof(*queued)) {
        return SDL_SetError("Noodles fill command data is not contiguous");
    }
    if (!old_count) {
        cmd->data.draw.first = first;
    }
    for (i = 0; i < count; ++i) {
        queued[i].x = (int)rects[i].x;
        queued[i].y = (int)rects[i].y;
        queued[i].w = SDL_max((int)rects[i].w, 1);
        queued[i].h = SDL_max((int)rects[i].h, 1);
    }
    cmd->data.draw.count = old_count + (size_t)count;
    return 0;
}

static int NOODLES_QueueCopyImpl(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
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

static int NOODLES_QueueCopyExImpl(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
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

static int NOODLES_QueueGeometryImpl(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
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

static void NOODLES_RecordBuild(NOODLES_RenderData *data, Uint64 start,
                                Uint64 *category)
{
    if (data->stats_enabled) {
        data->stats_build_ticks += SDL_GetPerformanceCounter() - start;
        data->stats_build_calls++;
        (*category)++;
    }
}

static int NOODLES_QueueNoOp(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_QueueNoOpImpl(renderer, cmd);
    NOODLES_RecordBuild(data, start, &data->stats_build_state_calls);
    return result;
}

static int NOODLES_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                   const SDL_FPoint *points, int count)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_QueueDrawPointsImpl(renderer, cmd, points, count);
    NOODLES_RecordBuild(data, start, &data->stats_build_primitive_calls);
    return result;
}

static int NOODLES_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                  const SDL_FRect *rects, int count)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const SDL_bool merged = cmd->data.draw.count != 0;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_QueueFillRectsImpl(renderer, cmd, rects, count);
    if (data->stats_enabled && result == 0 && merged) {
        data->stats_build_fill_merges++;
    }
    NOODLES_RecordBuild(data, start, &data->stats_build_fill_calls);
    return result;
}

static int NOODLES_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                             SDL_Texture *texture, const SDL_Rect *srcrect,
                             const SDL_FRect *dstrect)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_QueueCopyImpl(renderer, cmd, texture, srcrect, dstrect);
    NOODLES_RecordBuild(data, start, &data->stats_build_copy_calls);
    return result;
}

static int NOODLES_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                               SDL_Texture *texture, const SDL_Rect *srcrect,
                               const SDL_FRect *dstrect, double angle,
                               const SDL_FPoint *center, SDL_RendererFlip flip,
                               float scale_x, float scale_y)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_QueueCopyExImpl(renderer, cmd, texture, srcrect,
                                               dstrect, angle, center, flip,
                                               scale_x, scale_y);
    NOODLES_RecordBuild(data, start, &data->stats_build_copy_ex_calls);
    return result;
}

static int NOODLES_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                 SDL_Texture *texture, const float *xy,
                                 int xy_stride, const SDL_Color *color,
                                 int color_stride, const float *uv, int uv_stride,
                                 int num_vertices, const void *indices,
                                 int num_indices, int size_indices,
                                 float scale_x, float scale_y)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    const Uint64 start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_QueueGeometryImpl(renderer, cmd, texture, xy,
                                                 xy_stride, color, color_stride,
                                                 uv, uv_stride, num_vertices,
                                                 indices, num_indices, size_indices,
                                                 scale_x, scale_y);
    NOODLES_RecordBuild(data, start, &data->stats_build_geometry_calls);
    return result;
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
    bounds.w = target->width;
    bounds.h = target->height;
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

static Uint32 NOODLES_FillBlendMode(SDL_BlendMode blend)
{
    if (blend == SDL_BLENDMODE_NONE) {
        return NOODLES_DRAW_MODE_NONE;
    }
    if (blend == SDL_BLENDMODE_BLEND) {
        return NOODLES_DRAW_MODE_BLEND;
    }
    return NOODLES_BlendFlags(blend);
}

static int NOODLES_SoftwareCopy(NOODLES_RenderData *data, NOODLES_TextureData *target,
                                const SDL_RenderCommand *cmd, SDL_Rect source,
                                SDL_Rect destination, SDL_RendererFlip flip,
                                const SDL_Rect *viewport, const SDL_Rect *clip,
                                SDL_bool clip_enabled)
{
    NOODLES_TextureData *texturedata =
        (NOODLES_TextureData *)cmd->data.draw.texture->driverdata;
    SDL_Surface *copy_source;
    SDL_Surface *temporary = NULL;
    SDL_Rect effective_clip;
    int result;

    if (NOODLES_EnsureCPU(data, texturedata) < 0 ||
        NOODLES_EnsureCPU(data, target) < 0) {
        return -1;
    }
    copy_source = texturedata->shadow;
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
    if (data->stats_enabled) {
        SDL_Rect visible;
        data->stats_software_copies++;
        if (SDL_IntersectRect(&destination, &effective_clip, &visible)) {
            data->stats_software_copy_pixels += (Uint64)visible.w * visible.h;
        }
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
    NOODLES_AlphaState target_alpha;
    SDL_BlendMode effective_blend;
    SDL_bool alpha_skip;
    SDL_bool alpha_copy;
    Uint64 pixels;
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

    pixels = (Uint64)source.w * source.h;
    alpha_skip = cmd->data.draw.blend == SDL_BLENDMODE_BLEND &&
                 texturedata->alpha_state == NOODLES_ALPHA_ZERO;
    alpha_copy = cmd->data.draw.blend == SDL_BLENDMODE_BLEND &&
                 texturedata->alpha_state == NOODLES_ALPHA_OPAQUE &&
                 cmd->data.draw.r == 255 && cmd->data.draw.g == 255 &&
                 cmd->data.draw.b == 255 && cmd->data.draw.a == 255 &&
                 flip == SDL_FLIP_NONE;
    if (alpha_skip) {
        if (data->stats_enabled) {
            data->stats_alpha_skipped_draws++;
            data->stats_alpha_skipped_pixels += pixels;
        }
        return 0;
    }
    target_alpha = NOODLES_CopyAlphaState(target, &destination,
                                           texturedata->alpha_state,
                                           cmd->data.draw.a,
                                           cmd->data.draw.blend);

    if (NOODLES_EnsureGPU(data, target) < 0 ||
        NOODLES_EnsureGPU(data, texturedata) < 0) {
        return -1;
    }

    effective_blend = alpha_copy ? SDL_BLENDMODE_NONE : cmd->data.draw.blend;
    flags = NOODLES_BlendFlags(effective_blend);
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

    if (data->stats_enabled && cmd->data.draw.blend == SDL_BLENDMODE_BLEND &&
        cmd->data.draw.a == 255) {
        if (texturedata->shadow && texturedata->cpu_valid && source.w >= 2) {
            int y;
            data->stats_alpha_sample_draws++;
            data->stats_alpha_sample_pixels += pixels;
            for (y = 0; y < source.h; y += 8) {
                const Uint32 *row = (const Uint32 *)((const Uint8 *)texturedata->shadow->pixels +
                    (source.y + y) * texturedata->shadow->pitch) + source.x;
                int x;
                for (x = 0; x + 1 < source.w; x += 32) {
                    const Uint32 a0 = row[x] >> 24;
                    const Uint32 a1 = row[x + 1] >> 24;
                    if (a0 == 255 && a1 == 255) {
                        data->stats_alpha_pair_opaque++;
                    } else if (a0 == 0 && a1 == 0) {
                        data->stats_alpha_pair_zero++;
                    } else {
                        data->stats_alpha_pair_mixed++;
                    }
                }
            }
        } else {
            data->stats_alpha_unavailable_draws++;
            data->stats_alpha_unavailable_pixels += pixels;
        }
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
    if (NOODLES_SubmitDraw(data,
            target == &data->composition ? NULL : target->surface, &draw) < 0) {
        return -1;
    }
    if (data->stats_enabled && alpha_copy) {
        data->stats_alpha_copy_draws++;
        data->stats_alpha_copy_pixels += pixels;
    }
    texturedata->gpu_in_use = SDL_TRUE;
    NOODLES_MarkGPUWriteState(target, target_alpha);
    return 0;
}

static SDL_bool NOODLES_PrimitiveRegion(const SDL_Point *points, int count,
                                        const SDL_Rect *clip, SDL_Rect *region)
{
    SDL_Rect bounds;
    int min_x, min_y, max_x, max_y;
    int i;
    if (count <= 0) {
        return SDL_FALSE;
    }
    min_x = max_x = points[0].x;
    min_y = max_y = points[0].y;
    for (i = 1; i < count; ++i) {
        min_x = SDL_min(min_x, points[i].x);
        min_y = SDL_min(min_y, points[i].y);
        max_x = SDL_max(max_x, points[i].x);
        max_y = SDL_max(max_y, points[i].y);
    }
    bounds.x = min_x;
    bounds.y = min_y;
    bounds.w = max_x - min_x + 1;
    bounds.h = max_y - min_y + 1;
    return SDL_IntersectRect(&bounds, clip, region);
}

static SDL_bool NOODLES_CanFillLines(const SDL_Point *points, int count,
                                     const SDL_Rect *clip)
{
    int i;
    for (i = 1; i < count; ++i) {
        int x1 = points[i - 1].x;
        int y1 = points[i - 1].y;
        int x2 = points[i].x;
        int y2 = points[i].y;
        if (!SDL_IntersectRectAndLine(clip, &x1, &y1, &x2, &y2)) {
            continue;
        }
        if (x1 != x2 && y1 != y2 && SDL_abs(x2 - x1) != SDL_abs(y2 - y1)) {
            return SDL_FALSE;
        }
    }
    return SDL_TRUE;
}

static int NOODLES_SubmitOpaquePrimitives(NOODLES_RenderData *data,
                                          NOODLES_TextureData *target,
                                          SDL_RenderCommandType command,
                                          const SDL_Point *points, int count,
                                          const SDL_Rect *clip, Uint32 color)
{
    SDL_bool drew = SDL_FALSE;
    NOODLES_AlphaState alpha_state = target->alpha_state;
    const Uint8 alpha = (Uint8)(color >> 24);
    int i;
    if (NOODLES_EnsureGPU(data, target) < 0) {
        return -1;
    }
    if (command == SDL_RENDERCMD_DRAW_POINTS) {
        for (i = 0; i < count; ++i) {
            noodles_rect_t fill;
            if (!SDL_PointInRect(&points[i], clip)) {
                continue;
            }
            fill.x = points[i].x;
            fill.y = points[i].y;
            fill.width = 1;
            fill.height = 1;
            if (NOODLES_SubmitFill(data, target, &fill, color) < 0) {
                return -1;
            }
            {
                const SDL_Rect written = { fill.x, fill.y,
                                           (int)fill.width, (int)fill.height };
                alpha_state = NOODLES_FillAlphaState(target, alpha_state,
                                                     &written, alpha,
                                                     SDL_BLENDMODE_NONE);
            }
            drew = SDL_TRUE;
        }
    } else {
        for (i = 1; i < count; ++i) {
            int x1 = points[i - 1].x;
            int y1 = points[i - 1].y;
            int x2 = points[i].x;
            int y2 = points[i].y;
            noodles_rect_t fill;
            if (!SDL_IntersectRectAndLine(clip, &x1, &y1, &x2, &y2)) {
                continue;
            }
            if (y1 == y2) {
                fill.x = SDL_min(x1, x2);
                fill.y = y1;
                fill.width = (Uint32)(SDL_abs(x2 - x1) + 1);
                fill.height = 1;
                if (NOODLES_SubmitFill(data, target, &fill, color) < 0) {
                    return -1;
                }
            } else if (x1 == x2) {
                fill.x = x1;
                fill.y = SDL_min(y1, y2);
                fill.width = 1;
                fill.height = (Uint32)(SDL_abs(y2 - y1) + 1);
                if (NOODLES_SubmitFill(data, target, &fill, color) < 0) {
                    return -1;
                }
            } else {
                const int step_x = x2 > x1 ? 1 : -1;
                const int step_y = y2 > y1 ? 1 : -1;
                const int steps = SDL_abs(x2 - x1);
                int step;
                fill.width = 1;
                fill.height = 1;
                for (step = 0; step <= steps; ++step) {
                    fill.x = x1 + step * step_x;
                    fill.y = y1 + step * step_y;
                    if (NOODLES_SubmitFill(data, target, &fill, color) < 0) {
                        return -1;
                    }
                }
            }
            {
                const SDL_Rect written = { fill.x, fill.y,
                                           (int)fill.width, (int)fill.height };
                alpha_state = NOODLES_FillAlphaState(target, alpha_state,
                                                     &written, alpha,
                                                     SDL_BLENDMODE_NONE);
            }
            drew = SDL_TRUE;
        }
    }
    if (drew) {
        NOODLES_MarkGPUWriteState(target, alpha_state);
    }
    return 0;
}

static int NOODLES_RunCommandQueueImpl(SDL_Renderer *renderer, SDL_RenderCommand *cmd,
                                       void *vertices, size_t vertsize)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    NOODLES_TextureData *target = data->target;
    SDL_Rect viewport = { 0, 0, target->width, target->height };
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
            rect.width = (Uint32)target->width;
            rect.height = (Uint32)target->height;
            if (NOODLES_SubmitFill(data, target, &rect,
                    NOODLES_PackRGBA(cmd->data.color.r, cmd->data.color.g,
                                     cmd->data.color.b, cmd->data.color.a)) < 0) {
                return -1;
            }
            NOODLES_MarkGPUWriteState(target,
                                      NOODLES_AlphaFromByte(cmd->data.color.a));
            break;
        }
        case SDL_RENDERCMD_FILL_RECTS: {
            SDL_Rect *rects = (SDL_Rect *)((Uint8 *)vertices + cmd->data.draw.first);
            SDL_Rect effective_clip;
            NOODLES_AlphaState alpha_state = target->alpha_state;
            SDL_bool drew = SDL_FALSE;
            size_t i;
            if (!NOODLES_EffectiveClip(target, &viewport, &clip, clip_enabled,
                                       &effective_clip)) {
                break;
            }
            if (!NOODLES_SupportsBlendMode(renderer, cmd->data.draw.blend)) {
                Uint64 pixels = 0;
                SDL_Rect dirty = { 0, 0, 0, 0 };
                SDL_bool have_dirty = SDL_FALSE;
                if (!NOODLES_SoftwareBlendMode(cmd->data.draw.blend)) {
                    return SDL_SetError("Noodles cannot apply the requested fill blend mode");
                }
                for (i = 0; i < cmd->data.draw.count; ++i) {
                    SDL_Rect destination = rects[i];
                    SDL_Rect visible;
                    destination.x += viewport.x;
                    destination.y += viewport.y;
                    if (SDL_IntersectRect(&destination, &effective_clip, &visible)) {
                        if (have_dirty) {
                            SDL_UnionRect(&dirty, &visible, &dirty);
                        } else {
                            dirty = visible;
                            have_dirty = SDL_TRUE;
                        }
                    }
                }
                if (!have_dirty) {
                    break;
                }
                if (NOODLES_PrepareCPURegion(data, target, &dirty) < 0) {
                    return -1;
                }
                SDL_SetClipRect(target->shadow, &effective_clip);
                for (i = 0; i < cmd->data.draw.count; ++i) {
                    SDL_Rect destination = rects[i];
                    SDL_Rect visible;
                    destination.x += viewport.x;
                    destination.y += viewport.y;
                    if (SDL_BlendFillRect(target->shadow, &destination,
                                          cmd->data.draw.blend,
                                          cmd->data.draw.r, cmd->data.draw.g,
                                          cmd->data.draw.b, cmd->data.draw.a) < 0) {
                        return -1;
                    }
                    if (SDL_IntersectRect(&destination, &effective_clip, &visible)) {
                        pixels += (Uint64)visible.w * visible.h;
                    }
                }
                if (data->stats_enabled) {
                    data->stats_software_fills += cmd->data.draw.count;
                    data->stats_software_fill_pixels += pixels;
                }
                if (NOODLES_CommitCPURegion(data, target, &dirty) < 0) {
                    return -1;
                }
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
                if ((cmd->data.draw.blend == SDL_BLENDMODE_NONE
                        ? NOODLES_SubmitFill(data, target, &fill,
                            NOODLES_PackRGBA(cmd->data.draw.r, cmd->data.draw.g,
                                             cmd->data.draw.b, cmd->data.draw.a))
                        : NOODLES_SubmitBlendFill(data, target, &fill,
                            NOODLES_PackRGBA(cmd->data.draw.r, cmd->data.draw.g,
                                             cmd->data.draw.b, cmd->data.draw.a),
                            NOODLES_FillBlendMode(cmd->data.draw.blend))) < 0) {
                    return -1;
                }
                alpha_state = NOODLES_FillAlphaState(target, alpha_state,
                                                     &visible,
                                                     cmd->data.draw.a,
                                                     cmd->data.draw.blend);
                drew = SDL_TRUE;
            }
            if (drew) {
                NOODLES_MarkGPUWriteState(target, alpha_state);
            }
            break;
        }
        case SDL_RENDERCMD_DRAW_POINTS:
        case SDL_RENDERCMD_DRAW_LINES: {
            SDL_Point *points = (SDL_Point *)((Uint8 *)vertices + cmd->data.draw.first);
            SDL_Rect effective_clip;
            SDL_Rect dirty;
            const int count = (int)cmd->data.draw.count;
            int i;
            int result;
            if (!NOODLES_SoftwareBlendMode(cmd->data.draw.blend)) {
                return SDL_SetError("Noodles CPU primitive fallback cannot apply a custom blend mode");
            }
            if (!NOODLES_EffectiveClip(target, &viewport, &clip, clip_enabled,
                                       &effective_clip)) {
                break;
            }
            for (i = 0; i < count; ++i) {
                points[i].x += viewport.x;
                points[i].y += viewport.y;
            }
            if (cmd->data.draw.blend == SDL_BLENDMODE_NONE &&
                (cmd->command == SDL_RENDERCMD_DRAW_POINTS ||
                 NOODLES_CanFillLines(points, count, &effective_clip))) {
                if (NOODLES_SubmitOpaquePrimitives(data, target, cmd->command,
                        points, count, &effective_clip,
                        NOODLES_PackRGBA(cmd->data.draw.r, cmd->data.draw.g,
                                         cmd->data.draw.b, cmd->data.draw.a)) < 0) {
                    return -1;
                }
                break;
            }
            if (!NOODLES_PrimitiveRegion(points, count, &effective_clip, &dirty)) {
                break;
            }
            if (NOODLES_PrepareCPURegion(data, target, &dirty) < 0) {
                return -1;
            }
            SDL_SetClipRect(target->shadow, &effective_clip);
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
            if (data->stats_enabled) {
                data->stats_primitive_commands++;
                data->stats_primitive_vertices += cmd->data.draw.count;
            }
            if (NOODLES_CommitCPURegion(data, target, &dirty) < 0) {
                return -1;
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
            if (data->stats_enabled) {
                data->stats_geometry_commands++;
                data->stats_geometry_triangles += cmd->data.draw.count / 3u;
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
    data->in_command_queue = SDL_TRUE;
    result = NOODLES_RunCommandQueueImpl(renderer, cmd, vertices, vertsize);
    if (result == 0) {
        result = NOODLES_FlushQueued(data);
    }
    data->in_command_queue = SDL_FALSE;
    if (data->stats_enabled) {
        const Uint64 elapsed = SDL_GetPerformanceCounter() - start;
        data->stats_queue_ticks += elapsed;
        data->stats_queue_max_ticks = SDL_max(data->stats_queue_max_ticks, elapsed);
    }
    return result;
}

static int NOODLES_RenderReadPixelsImpl(SDL_Renderer *renderer, const SDL_Rect *rect,
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

static int NOODLES_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                                    Uint32 format, void *pixels, int pitch)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    Uint64 start;
    if (NOODLES_WaitPresent(data) < 0) {
        return -1;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    const int result = NOODLES_RenderReadPixelsImpl(renderer, rect, format,
                                                    pixels, pitch);
    if (data->stats_enabled) {
        data->stats_read_pixels_ticks += SDL_GetPerformanceCounter() - start;
        data->stats_read_pixels_calls++;
        data->stats_read_pixels_bytes += (Uint64)rect->w * rect->h * 4u;
    }
    return result;
}

static int NOODLES_RenderPresent(SDL_Renderer *renderer)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    noodles_fence_t fence;
    Uint64 start;
    if (NOODLES_WaitPresent(data) < 0) {
        return -1;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    if (NOODLES_FlushQueued(data) < 0) {
        return -1;
    }
    if (NOODLES_EnsureGPU(data, &data->composition) < 0) {
        return -1;
    }
    data->present_draw_fence = noodles_link_last_fence(data->link);
    if (noodles_push_present(data->link, &fence) < 0) {
        return NOODLES_SetErrno("present");
    }
    data->present_fence = fence;
    data->present_pending = SDL_TRUE;
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
                                 (double)data->stats_present_ticks -
                                 (double)data->stats_present_wait_ticks;
            double queue_other_ticks = (double)data->stats_queue_ticks -
                                       (double)data->stats_fill_ticks -
                                       (double)data->stats_draw_ticks -
                                       (double)data->stats_queue_sync_ticks;
            double present_other_ticks = (double)data->stats_present_ticks -
                                         (double)data->stats_present_copy_ticks;
            if (other_ticks < 0.0) {
                other_ticks = 0.0;
            }
            if (queue_other_ticks < 0.0) {
                queue_other_ticks = 0.0;
            }
            if (present_other_ticks < 0.0) {
                present_other_ticks = 0.0;
            }
            SDL_Log("Noodles stats: frames=%llu fps=%.2f queue=%.3f ms avg/%.3f max present=%.3f ms avg/%.3f max other=%.3f ms avg",
                    (unsigned long long)data->stats_frames,
                    frame_count * (double)frequency / (double)interval_ticks,
                    (double)data->stats_queue_ticks * milliseconds / frame_count,
                    (double)data->stats_queue_max_ticks * milliseconds,
                    (double)data->stats_present_ticks * milliseconds / frame_count,
                    (double)data->stats_present_max_ticks * milliseconds,
                    other_ticks * milliseconds / frame_count);
            SDL_Log("Noodles timing: fill=%.3f ms draw=%.3f ms queue-sync=%.3f ms queue-other=%.3f ms sync-total=%.3f ms present-copy=%.3f ms present-wait=%.3f ms/%llu waits/%.3f max present-other=%.3f ms avg",
                    (double)data->stats_fill_ticks * milliseconds / frame_count,
                    (double)data->stats_draw_ticks * milliseconds / frame_count,
                    (double)data->stats_queue_sync_ticks * milliseconds / frame_count,
                    queue_other_ticks * milliseconds / frame_count,
                    (double)data->stats_sync_ticks * milliseconds / frame_count,
                    (double)data->stats_present_copy_ticks * milliseconds / frame_count,
                    (double)data->stats_present_wait_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_present_waits,
                    (double)data->stats_present_wait_max_ticks * milliseconds,
                    present_other_ticks * milliseconds / frame_count);
            {
                Uint64 user = 0, system = 0, voluntary = 0, involuntary = 0;
                const long clock_ticks = sysconf(_SC_CLK_TCK);
                const double seconds = (double)interval_ticks / (double)frequency;
                const int usage = NOODLES_ReadThreadUsage(&user, &system, &voluntary,
                                                          &involuntary);
                const SDL_bool have_usage = usage == 0 && data->stats_thread_user +
                    data->stats_thread_system + data->stats_thread_voluntary != 0;
                SDL_Log("Noodles pacing: present-wait draw=%.3f ms flip=%.3f ms avg/%.3f max drawn-before-wait=%llu/%llu; main thread user=%.1f%% sys=%.1f%% voluntary-switches=%.1f involuntary=%.1f per frame",
                        (double)data->stats_present_draw_wait_ticks * milliseconds / frame_count,
                        (double)data->stats_present_flip_wait_ticks * milliseconds / frame_count,
                        (double)data->stats_present_flip_wait_max_ticks * milliseconds,
                        (unsigned long long)data->stats_present_drawn_early,
                        (unsigned long long)data->stats_present_waits,
                        have_usage ? (double)(user - data->stats_thread_user) * 100.0 /
                                     (double)clock_ticks / seconds : 0.0,
                        have_usage ? (double)(system - data->stats_thread_system) * 100.0 /
                                     (double)clock_ticks / seconds : 0.0,
                        have_usage ? (double)(voluntary - data->stats_thread_voluntary) /
                                     frame_count : 0.0,
                        have_usage ? (double)(involuntary - data->stats_thread_involuntary) /
                                     frame_count : 0.0);
                if (usage == 0) {
                    data->stats_thread_user = user;
                    data->stats_thread_system = system;
                    data->stats_thread_voluntary = voluntary;
                    data->stats_thread_involuntary = involuntary;
                }
            }
            SDL_Log("Noodles work: fill=%llu/%.3fMpx batches=%llu max-batch=%llu stalls=%llu blend-fill=%llu/%.3fMpx stalls=%llu plain=%llu/%.3fMpx flagged=%llu/%.3fMpx draw-batches=%llu max-draw-batch=%llu draw-stalls=%llu CPU-copy=%llu/%.3fMpx CPU-fill=%llu/%.3fMpx primitives=%llu/%llu vertices geometry=%llu/%llu triangles",
                    (unsigned long long)data->stats_fill_commands,
                    (double)data->stats_fill_pixels / 1000000.0,
                    (unsigned long long)data->stats_fill_batches,
                    (unsigned long long)data->stats_fill_batch_max,
                    (unsigned long long)data->stats_fill_stalls,
                    (unsigned long long)data->stats_blend_fill_commands,
                    (double)data->stats_blend_fill_pixels / 1000000.0,
                    (unsigned long long)data->stats_blend_fill_stalls,
                    (unsigned long long)data->stats_plain_draws,
                    (double)data->stats_plain_draw_pixels / 1000000.0,
                    (unsigned long long)data->stats_flagged_draws,
                    (double)data->stats_flagged_draw_pixels / 1000000.0,
                    (unsigned long long)data->stats_draw_batches,
                    (unsigned long long)data->stats_draw_batch_max,
                    (unsigned long long)data->stats_draw_stalls,
                    (unsigned long long)data->stats_software_copies,
                    (double)data->stats_software_copy_pixels / 1000000.0,
                    (unsigned long long)data->stats_software_fills,
                    (double)data->stats_software_fill_pixels / 1000000.0,
                    (unsigned long long)data->stats_primitive_commands,
                    (unsigned long long)data->stats_primitive_vertices,
                    (unsigned long long)data->stats_geometry_commands,
                    (unsigned long long)data->stats_geometry_triangles);
            SDL_Log("Noodles draw classes: copy64=%llu/%.3fMpx reroute=%llu/%.3fMpx blend=%llu/%.3fMpx replace-mod=%llu/%.3fMpx add=%llu/%.3fMpx mod=%llu/%.3fMpx mul=%llu/%.3fMpx custom=%llu/%.3fMpx mirrored=%llu/%.3fMpx modulated=%llu/%.3fMpx",
                    (unsigned long long)data->stats_draw_class_commands[NOODLES_DRAW_CLASS_COPY64],
                    (double)data->stats_draw_class_pixels[NOODLES_DRAW_CLASS_COPY64] / 1000000.0,
                    (unsigned long long)data->stats_draw_class_commands[NOODLES_DRAW_CLASS_PLAIN_REROUTE],
                    (double)data->stats_draw_class_pixels[NOODLES_DRAW_CLASS_PLAIN_REROUTE] / 1000000.0,
                    (unsigned long long)data->stats_draw_class_commands[NOODLES_DRAW_CLASS_BLEND],
                    (double)data->stats_draw_class_pixels[NOODLES_DRAW_CLASS_BLEND] / 1000000.0,
                    (unsigned long long)data->stats_draw_class_commands[NOODLES_DRAW_CLASS_REPLACE_MOD],
                    (double)data->stats_draw_class_pixels[NOODLES_DRAW_CLASS_REPLACE_MOD] / 1000000.0,
                    (unsigned long long)data->stats_draw_class_commands[NOODLES_DRAW_CLASS_ADD],
                    (double)data->stats_draw_class_pixels[NOODLES_DRAW_CLASS_ADD] / 1000000.0,
                    (unsigned long long)data->stats_draw_class_commands[NOODLES_DRAW_CLASS_MOD],
                    (double)data->stats_draw_class_pixels[NOODLES_DRAW_CLASS_MOD] / 1000000.0,
                    (unsigned long long)data->stats_draw_class_commands[NOODLES_DRAW_CLASS_MUL],
                    (double)data->stats_draw_class_pixels[NOODLES_DRAW_CLASS_MUL] / 1000000.0,
                    (unsigned long long)data->stats_draw_class_commands[NOODLES_DRAW_CLASS_CUSTOM],
                    (double)data->stats_draw_class_pixels[NOODLES_DRAW_CLASS_CUSTOM] / 1000000.0,
                    (unsigned long long)data->stats_mirrored_draws,
                    (double)data->stats_mirrored_draw_pixels / 1000000.0,
                    (unsigned long long)data->stats_modulated_draws,
                    (double)data->stats_modulated_draw_pixels / 1000000.0);
            SDL_Log("Noodles size buckets: draw <=64=%llu/%.3fMpx <=1K=%llu/%.3fMpx <=4K=%llu/%.3fMpx <=16K=%llu/%.3fMpx >16K=%llu/%.3fMpx; fill <=64=%llu/%.3fMpx <=1K=%llu/%.3fMpx <=4K=%llu/%.3fMpx <=16K=%llu/%.3fMpx >16K=%llu/%.3fMpx",
                    (unsigned long long)data->stats_draw_size_commands[0],
                    (double)data->stats_draw_size_pixels[0] / 1000000.0,
                    (unsigned long long)data->stats_draw_size_commands[1],
                    (double)data->stats_draw_size_pixels[1] / 1000000.0,
                    (unsigned long long)data->stats_draw_size_commands[2],
                    (double)data->stats_draw_size_pixels[2] / 1000000.0,
                    (unsigned long long)data->stats_draw_size_commands[3],
                    (double)data->stats_draw_size_pixels[3] / 1000000.0,
                    (unsigned long long)data->stats_draw_size_commands[4],
                    (double)data->stats_draw_size_pixels[4] / 1000000.0,
                    (unsigned long long)data->stats_fill_size_commands[0],
                    (double)data->stats_fill_size_pixels[0] / 1000000.0,
                    (unsigned long long)data->stats_fill_size_commands[1],
                    (double)data->stats_fill_size_pixels[1] / 1000000.0,
                    (unsigned long long)data->stats_fill_size_commands[2],
                    (double)data->stats_fill_size_pixels[2] / 1000000.0,
                    (unsigned long long)data->stats_fill_size_commands[3],
                    (double)data->stats_fill_size_pixels[3] / 1000000.0,
                    (unsigned long long)data->stats_fill_size_commands[4],
                    (double)data->stats_fill_size_pixels[4] / 1000000.0);
            SDL_Log("Noodles alpha: sample=%llu/%.3fMpx pairs opaque=%llu zero=%llu partial-or-edge=%llu unavailable=%llu/%.3fMpx skipped=%llu/%.3fMpx copied=%llu/%.3fMpx",
                    (unsigned long long)data->stats_alpha_sample_draws,
                    (double)data->stats_alpha_sample_pixels / 1000000.0,
                    (unsigned long long)data->stats_alpha_pair_opaque,
                    (unsigned long long)data->stats_alpha_pair_zero,
                    (unsigned long long)data->stats_alpha_pair_mixed,
                    (unsigned long long)data->stats_alpha_unavailable_draws,
                    (double)data->stats_alpha_unavailable_pixels / 1000000.0,
                    (unsigned long long)data->stats_alpha_skipped_draws,
                    (double)data->stats_alpha_skipped_pixels / 1000000.0,
                    (unsigned long long)data->stats_alpha_copy_draws,
                    (double)data->stats_alpha_copy_pixels / 1000000.0);
            SDL_Log("Noodles sync: uploads=%llu/%.3fMiB readbacks=%llu/%.3fMiB evictions=%llu drains=%llu progress=%llu resident=%.1fMiB shadow=%.1fMiB/%.1fMiB peak alloc=%llu free=%llu",
                    (unsigned long long)data->stats_uploads,
                    (double)data->stats_upload_bytes / (1024.0 * 1024.0),
                    (unsigned long long)data->stats_readbacks,
                    (double)data->stats_readback_bytes / (1024.0 * 1024.0),
                    (unsigned long long)data->stats_evictions,
                    (unsigned long long)data->stats_drains,
                    (unsigned long long)data->stats_progress_waits,
                    (double)data->resident_bytes / (1024.0 * 1024.0),
                    (double)data->shadow_bytes / (1024.0 * 1024.0),
                    (double)data->shadow_peak_bytes / (1024.0 * 1024.0),
                    (unsigned long long)data->stats_shadow_allocations,
                    (unsigned long long)data->stats_shadow_releases);
            SDL_Log("Noodles callbacks: build=%.3fms/%llu calls (state=%llu primitive=%llu fill=%llu merged=%llu copy=%llu copyex=%llu geometry=%llu) create=%.3fms/%llu update=%.3fms/%llu/%.3fMiB deferred=%llu rotated=%llu lock=%.3fms/%llu/%.3fMiB unlock=%.3fms/%llu target=%.3fms/%llu read=%.3fms/%llu/%.3fMiB destroy=%.3fms/%llu avg/frame",
                    (double)data->stats_build_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_build_calls,
                    (unsigned long long)data->stats_build_state_calls,
                    (unsigned long long)data->stats_build_primitive_calls,
                    (unsigned long long)data->stats_build_fill_calls,
                    (unsigned long long)data->stats_build_fill_merges,
                    (unsigned long long)data->stats_build_copy_calls,
                    (unsigned long long)data->stats_build_copy_ex_calls,
                    (unsigned long long)data->stats_build_geometry_calls,
                    (double)data->stats_create_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_create_calls,
                    (double)data->stats_update_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_update_calls,
                    (double)data->stats_update_bytes / (1024.0 * 1024.0),
                    (unsigned long long)data->stats_update_deferred,
                    (unsigned long long)data->stats_update_rotated,
                    (double)data->stats_lock_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_lock_calls,
                    (double)data->stats_lock_bytes / (1024.0 * 1024.0),
                    (double)data->stats_unlock_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_unlock_calls,
                    (double)data->stats_target_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_target_calls,
                    (double)data->stats_read_pixels_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_read_pixels_calls,
                    (double)data->stats_read_pixels_bytes / (1024.0 * 1024.0),
                    (double)data->stats_destroy_ticks * milliseconds / frame_count,
                    (unsigned long long)data->stats_destroy_calls);
            data->stats_start = now;
            data->stats_frames = 0;
            data->stats_queue_ticks = 0;
            data->stats_queue_max_ticks = 0;
            data->stats_present_ticks = 0;
            data->stats_present_max_ticks = 0;
            data->stats_fill_ticks = 0;
            data->stats_draw_ticks = 0;
            data->stats_sync_ticks = 0;
            data->stats_queue_sync_ticks = 0;
            data->stats_present_copy_ticks = 0;
            data->stats_present_wait_ticks = 0;
            data->stats_present_wait_max_ticks = 0;
            data->stats_present_waits = 0;
            data->stats_present_draw_wait_ticks = 0;
            data->stats_present_flip_wait_ticks = 0;
            data->stats_present_flip_wait_max_ticks = 0;
            data->stats_present_drawn_early = 0;
            data->stats_fill_commands = 0;
            data->stats_fill_pixels = 0;
            data->stats_fill_stalls = 0;
            data->stats_fill_batches = 0;
            data->stats_fill_batch_max = 0;
            data->stats_blend_fill_commands = 0;
            data->stats_blend_fill_pixels = 0;
            data->stats_blend_fill_stalls = 0;
            data->stats_plain_draws = 0;
            data->stats_plain_draw_pixels = 0;
            data->stats_flagged_draws = 0;
            data->stats_flagged_draw_pixels = 0;
            SDL_memset(data->stats_draw_class_commands, 0,
                       sizeof(data->stats_draw_class_commands));
            SDL_memset(data->stats_draw_class_pixels, 0,
                       sizeof(data->stats_draw_class_pixels));
            SDL_memset(data->stats_draw_size_commands, 0,
                       sizeof(data->stats_draw_size_commands));
            SDL_memset(data->stats_draw_size_pixels, 0,
                       sizeof(data->stats_draw_size_pixels));
            SDL_memset(data->stats_fill_size_commands, 0,
                       sizeof(data->stats_fill_size_commands));
            SDL_memset(data->stats_fill_size_pixels, 0,
                       sizeof(data->stats_fill_size_pixels));
            data->stats_mirrored_draws = 0;
            data->stats_mirrored_draw_pixels = 0;
            data->stats_modulated_draws = 0;
            data->stats_modulated_draw_pixels = 0;
            data->stats_alpha_sample_draws = 0;
            data->stats_alpha_sample_pixels = 0;
            data->stats_alpha_pair_opaque = 0;
            data->stats_alpha_pair_zero = 0;
            data->stats_alpha_pair_mixed = 0;
            data->stats_alpha_unavailable_draws = 0;
            data->stats_alpha_unavailable_pixels = 0;
            data->stats_alpha_skipped_draws = 0;
            data->stats_alpha_skipped_pixels = 0;
            data->stats_alpha_copy_draws = 0;
            data->stats_alpha_copy_pixels = 0;
            data->stats_draw_stalls = 0;
            data->stats_draw_batches = 0;
            data->stats_draw_batch_max = 0;
            data->stats_software_copies = 0;
            data->stats_software_copy_pixels = 0;
            data->stats_software_fills = 0;
            data->stats_software_fill_pixels = 0;
            data->stats_primitive_commands = 0;
            data->stats_primitive_vertices = 0;
            data->stats_geometry_commands = 0;
            data->stats_geometry_triangles = 0;
            data->stats_uploads = 0;
            data->stats_upload_bytes = 0;
            data->stats_readbacks = 0;
            data->stats_readback_bytes = 0;
            data->stats_evictions = 0;
            data->stats_drains = 0;
            data->stats_progress_waits = 0;
            data->stats_shadow_allocations = 0;
            data->stats_shadow_releases = 0;
            data->stats_build_ticks = 0;
            data->stats_build_calls = 0;
            data->stats_build_state_calls = 0;
            data->stats_build_primitive_calls = 0;
            data->stats_build_fill_calls = 0;
            data->stats_build_fill_merges = 0;
            data->stats_build_copy_calls = 0;
            data->stats_build_copy_ex_calls = 0;
            data->stats_build_geometry_calls = 0;
            data->stats_create_ticks = 0;
            data->stats_create_calls = 0;
            data->stats_update_ticks = 0;
            data->stats_update_calls = 0;
            data->stats_update_bytes = 0;
            data->stats_update_deferred = 0;
            data->stats_update_rotated = 0;
            data->stats_lock_ticks = 0;
            data->stats_lock_calls = 0;
            data->stats_lock_bytes = 0;
            data->stats_unlock_ticks = 0;
            data->stats_unlock_calls = 0;
            data->stats_target_ticks = 0;
            data->stats_target_calls = 0;
            data->stats_read_pixels_ticks = 0;
            data->stats_read_pixels_calls = 0;
            data->stats_read_pixels_bytes = 0;
            data->stats_destroy_ticks = 0;
            data->stats_destroy_calls = 0;
            data->shadow_peak_bytes = data->shadow_bytes;
        }
    }
    /* SDL leaves the back buffer undefined after presentation. Completion is
       deferred until the next link user so CPU frame preparation can overlap
       FPGA rendering and vertical blank. */
    data->composition.gpu_valid = SDL_TRUE;
    data->composition.cpu_valid = SDL_FALSE;
    data->composition.alpha_state = NOODLES_ALPHA_UNKNOWN;
    return 0;
}

static void NOODLES_DestroyTextureImpl(SDL_Renderer *renderer, SDL_Texture *texture)
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

static void NOODLES_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    Uint64 start;
    if (NOODLES_WaitPresent(data) < 0) {
        return;
    }
    start = data->stats_enabled ? SDL_GetPerformanceCounter() : 0;
    NOODLES_DestroyTextureImpl(renderer, texture);
    if (data->stats_enabled) {
        data->stats_destroy_ticks += SDL_GetPerformanceCounter() - start;
        data->stats_destroy_calls++;
    }
}

static void NOODLES_DestroyRenderer(SDL_Renderer *renderer)
{
    NOODLES_RenderData *data = (NOODLES_RenderData *)renderer->driverdata;
    if (!data) {
        return;
    }
    (void)NOODLES_WaitPresent(data);
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
        NOODLES_ReadThreadUsage(&data->stats_thread_user, &data->stats_thread_system,
                                &data->stats_thread_voluntary,
                                &data->stats_thread_involuntary);
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
        !(info.opcode_mask & NOODLES_CAP_BLIT_BLEND) ||
        !(info.opcode_mask & NOODLES_CAP_BLEND_FILL)) {
        NOODLES_DestroyRenderer(renderer);
        return SDL_SetError("Noodles renderer requires protocol 1.4");
    }
    data->fill_batch_enabled =
        (info.opcode_mask & NOODLES_CAP_FILL_BATCH) != 0;
    if (NOODLES_InitSurface(data, NOODLES_BUFFER_WIDTH, NOODLES_BUFFER_HEIGHT,
                            SDL_TRUE, &data->composition) < 0) {
        NOODLES_DestroyRenderer(renderer);
        return -1;
    }
    data->composition.cpu_valid = SDL_FALSE;
    data->composition.gpu_valid = SDL_TRUE;
    data->composition.alpha_state = NOODLES_ALPHA_UNKNOWN;
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
    renderer->merge_fill_rects = SDL_TRUE;
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
