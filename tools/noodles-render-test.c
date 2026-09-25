#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 256
#define HEIGHT 128

enum {
    REF_ZERO = 1, REF_ONE, REF_SRC_COLOR, REF_ONE_MINUS_SRC_COLOR,
    REF_SRC_ALPHA, REF_ONE_MINUS_SRC_ALPHA, REF_DST_COLOR,
    REF_ONE_MINUS_DST_COLOR, REF_DST_ALPHA, REF_ONE_MINUS_DST_ALPHA
};
enum { REF_ADD = 1, REF_SUBTRACT, REF_REV_SUBTRACT, REF_MINIMUM, REF_MAXIMUM };

static uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | (uint32_t)a << 24;
}

static uint32_t factor(uint32_t which, uint32_t source, uint32_t source_alpha,
                       uint32_t destination, uint32_t destination_alpha)
{
    switch (which) {
    case REF_ZERO: return 0;
    case REF_ONE: return 255;
    case REF_SRC_COLOR: return source;
    case REF_ONE_MINUS_SRC_COLOR: return 255 - source;
    case REF_SRC_ALPHA: return source_alpha;
    case REF_ONE_MINUS_SRC_ALPHA: return 255 - source_alpha;
    case REF_DST_COLOR: return destination;
    case REF_ONE_MINUS_DST_COLOR: return 255 - destination;
    case REF_DST_ALPHA: return destination_alpha;
    default: return 255 - destination_alpha;
    }
}

static uint32_t channel(uint32_t source, uint32_t source_alpha, uint32_t destination,
                        uint32_t destination_alpha, uint32_t source_factor,
                        uint32_t destination_factor, uint32_t operation, int single_rounding)
{
    uint32_t ps = source * factor(source_factor, source, source_alpha,
                                  destination, destination_alpha);
    uint32_t pd = destination * factor(destination_factor, source, source_alpha,
                                       destination, destination_alpha);
    int32_t s = (int32_t)(ps / 255u);
    int32_t d = (int32_t)(pd / 255u);
    int32_t out;
    switch (operation) {
    case REF_ADD: out = single_rounding ? (int32_t)((ps + pd) / 255u) : s + d; break;
    case REF_SUBTRACT: out = s - d; break;
    case REF_REV_SUBTRACT: out = d - s; break;
    case REF_MINIMUM: out = (int32_t)(source < destination ? source : destination); break;
    default: out = (int32_t)(source > destination ? source : destination); break;
    }
    return out < 0 ? 0u : out > 255 ? 255u : (uint32_t)out;
}

static uint32_t reference(uint32_t source, uint32_t destination, uint32_t modulation,
                          uint32_t color_source_factor, uint32_t color_destination_factor,
                          uint32_t color_operation, uint32_t alpha_source_factor,
                          uint32_t alpha_destination_factor, uint32_t alpha_operation,
                          int single_rounding)
{
    uint32_t result = 0;
    uint32_t source_alpha = ((source >> 24) * (modulation >> 24)) / 255u;
    uint32_t destination_alpha = destination >> 24;
    unsigned shift;
    result |= channel(source_alpha, source_alpha, destination_alpha, destination_alpha,
                      alpha_source_factor, alpha_destination_factor, alpha_operation,
                      single_rounding) << 24;
    for (shift = 0; shift < 24; shift += 8) {
        uint32_t source_channel = (((source >> shift) & 255u) *
                                   ((modulation >> shift) & 255u)) / 255u;
        uint32_t destination_channel = (destination >> shift) & 255u;
        result |= channel(source_channel, source_alpha, destination_channel,
                          destination_alpha, color_source_factor,
                          color_destination_factor, color_operation,
                          single_rounding) << shift;
    }
    return result;
}

static int check_pixel(const uint32_t *pixels, int x, int y, uint32_t expected,
                       const char *label)
{
    uint32_t actual = pixels[y * WIDTH + x];
    if (actual != expected) {
        fprintf(stderr, "%s at %d,%d: got %08x expected %08x\n",
                label, x, y, actual, expected);
        return -1;
    }
    return 0;
}

static int render_copy(SDL_Renderer *renderer, SDL_Texture *texture, int x,
                       SDL_BlendMode blend, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                       SDL_RendererFlip flip)
{
    SDL_Rect destination = { x, 0, 16, 16 };
    if (SDL_SetTextureBlendMode(texture, blend) < 0 ||
        SDL_SetTextureColorMod(texture, r, g, b) < 0 ||
        SDL_SetTextureAlphaMod(texture, a) < 0) {
        return -1;
    }
    return SDL_RenderCopyEx(renderer, texture, NULL, &destination, 0.0, NULL, flip);
}

static int draw_display_and_check(SDL_Renderer *renderer, SDL_Texture *target,
                                  uint32_t expected, const char *phase)
{
    SDL_Rect rect;
    uint32_t pixel;
    if (SDL_SetRenderTarget(renderer, NULL) < 0 ||
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) < 0 ||
        SDL_RenderClear(renderer) < 0) {
        fprintf(stderr, "%s display clear: %s\n", phase, SDL_GetError());
        return -1;
    }
    rect = (SDL_Rect){ (800 - WIDTH) / 2, (600 - HEIGHT) / 2, WIDTH, HEIGHT };
    if (SDL_SetTextureBlendMode(target, SDL_BLENDMODE_NONE) < 0 ||
        SDL_SetTextureColorMod(target, 255, 255, 255) < 0 ||
        SDL_SetTextureAlphaMod(target, 255) < 0 ||
        SDL_RenderCopy(renderer, target, NULL, &rect) < 0) {
        fprintf(stderr, "%s display copy: %s\n", phase, SDL_GetError());
        return -1;
    }
    rect = (SDL_Rect){ 10, 10, 4, 4 };
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) < 0 ||
        SDL_SetRenderDrawColor(renderer, 100, 50, 200, 128) < 0 ||
        SDL_RenderFillRect(renderer, &rect) < 0) {
        fprintf(stderr, "%s default-target regional fill: %s\n",
                phase, SDL_GetError());
        return -1;
    }
    rect = (SDL_Rect){ 10, 10, 1, 1 };
    if (SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_ABGR8888,
                             &pixel, 4) < 0 ||
        pixel != reference(rgba(100, 50, 200, 128), rgba(0, 0, 0, 255),
                           0xffffffffu, REF_SRC_ALPHA,
                           REF_ONE_MINUS_SRC_ALPHA, REF_ADD, REF_ONE,
                           REF_ONE_MINUS_SRC_ALPHA, REF_ADD, 0)) {
        fprintf(stderr, "%s default-target regional result: got %08x\n",
                phase, pixel);
        return -1;
    }
    rect = (SDL_Rect){ (800 - WIDTH) / 2 + 25,
                       (600 - HEIGHT) / 2 + 100, 1, 1 };
    if (SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_ABGR8888,
                             &pixel, 4) < 0 || pixel != expected) {
        fprintf(stderr,
                "%s default-target accelerated result: got %08x expected %08x\n",
                phase, pixel, expected);
        return -1;
    }
    return 0;
}

static SDL_AudioDeviceID queue_tone(void)
{
    enum { TONE_RATE = 48000, TONE_FRAMES = 48000, TONE_AMPLITUDE = 9000 };
    SDL_AudioSpec wanted;
    SDL_AudioSpec obtained;
    SDL_AudioDeviceID device;
    Sint16 *samples;
    uint32_t phase = 0;
    int i;

    SDL_zero(wanted);
    wanted.freq = TONE_RATE;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 1024;
    device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
    if (!device) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return 0;
    }
    if (obtained.freq != TONE_RATE || obtained.format != AUDIO_S16SYS ||
        obtained.channels != 2) {
        fprintf(stderr, "unexpected audio format: %d Hz format=%04x channels=%u\n",
                obtained.freq, obtained.format, obtained.channels);
        SDL_CloseAudioDevice(device);
        return 0;
    }
    samples = (Sint16 *)SDL_malloc(TONE_FRAMES * 2u * sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "tone allocation: %s\n", SDL_GetError());
        SDL_CloseAudioDevice(device);
        return 0;
    }
    for (i = 0; i < TONE_FRAMES; ++i) {
        int32_t triangle = phase < 24000u ? (int32_t)phase : 48000 - (int32_t)phase;
        Sint16 sample = (Sint16)((triangle * (TONE_AMPLITUDE * 2) / 24000) -
                                TONE_AMPLITUDE);
        samples[i * 2] = sample;
        samples[i * 2 + 1] = sample;
        phase = (phase + 440u) % 48000u;
    }
    if (SDL_QueueAudio(device, samples,
                       TONE_FRAMES * 2u * sizeof(*samples)) < 0) {
        fprintf(stderr, "SDL_QueueAudio: %s\n", SDL_GetError());
        SDL_free(samples);
        SDL_CloseAudioDevice(device);
        return 0;
    }
    SDL_free(samples);
    SDL_PauseAudioDevice(device, 0);
    printf("SDL audio diagnostic: queued 1.000 s, 48000 Hz stereo S16, driver=%s\n",
           SDL_GetCurrentAudioDriver());
    return device;
}

int main(int argc, char **argv)
{
    const uint32_t background = rgba(11, 23, 37, 201);
    const uint32_t p1 = rgba(200, 10, 20, 255);
    const uint32_t p2 = rgba(20, 210, 30, 192);
    const uint32_t p3 = rgba(30, 40, 220, 128);
    const uint32_t p4 = rgba(240, 230, 50, 64);
    const uint32_t modulation = rgba(128, 64, 255, 128);
    const uint32_t batch_fill = rgba(5, 101, 207, 255);
    const uint32_t fill_batch_second = rgba(193, 47, 83, 255);
    uint32_t source[16 * 16];
    uint32_t result[WIDTH * HEIGHT];
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_AudioDeviceID audio = 0;
    SDL_Texture *texture = NULL;
    SDL_Texture *target = NULL;
    SDL_Texture *lazy = NULL;
    SDL_Texture *residency[3] = { NULL, NULL, NULL };
    SDL_RendererInfo info;
    SDL_BlendMode stencil;
    SDL_Rect rect;
    SDL_Point line[2];
    SDL_Vertex triangle[3];
    uint32_t hash = 2166136261u;
    int failures = 0;
    int i, x, y;
    int rc = 1;
    unsigned hold_ms = 1500;

    if (argc == 3 && strcmp(argv[1], "--hold-ms") == 0) {
        hold_ms = (unsigned)strtoul(argv[2], NULL, 0);
    }
    for (y = 0; y < 16; ++y) {
        for (x = 0; x < 16; ++x) {
            source[y * 16 + x] = y < 8 ? (x < 8 ? p1 : p2) : (x < 8 ? p3 : p4);
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        goto done;
    }
    if (!SDL_getenv("NOODLES_TEST_SKIP_AUDIO")) {
        audio = queue_tone();
        if (!audio) {
            goto done;
        }
    }
    if (SDL_SetHint("SDL_RENDER_NOODLES_RESIDENT_MB", "12") != SDL_TRUE) {
        fprintf(stderr, "residency hint was rejected\n");
        goto done;
    }
    window = SDL_CreateWindow("Noodles renderer test", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        goto done;
    }
    for (i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        SDL_RendererInfo available;
        if (SDL_GetRenderDriverInfo(i, &available) == 0) {
            printf("SDL render driver %d: %s flags=%08x\n", i, available.name,
                   available.flags);
        }
    }
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE |
                                  SDL_RENDERER_PRESENTVSYNC);
    if (!renderer || SDL_GetRendererInfo(renderer, &info) < 0) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        goto done;
    }
    if (strcmp(info.name, "noodles") != 0) {
        fprintf(stderr, "selected renderer is %s, expected noodles\n", info.name);
        goto done;
    }
    if (SDL_RenderSetLogicalSize(renderer, 800, 600) < 0) {
        fprintf(stderr, "SDL_RenderSetLogicalSize: %s\n", SDL_GetError());
        goto done;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                SDL_TEXTUREACCESS_STATIC, 16, 16);
    target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                               SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
    if (!texture || !target || SDL_UpdateTexture(texture, NULL, source, 16 * 4) < 0) {
        fprintf(stderr, "texture setup: %s\n", SDL_GetError());
        goto done;
    }
    if (SDL_SetRenderTarget(renderer, target) < 0 ||
        SDL_SetRenderDrawColor(renderer, 11, 23, 37, 201) < 0 ||
        SDL_RenderClear(renderer) < 0) {
        fprintf(stderr, "target clear: %s\n", SDL_GetError());
        goto done;
    }

    rect = (SDL_Rect){ 220, 0, 16, 16 };
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE) < 0 ||
        SDL_SetRenderDrawColor(renderer, 70, 80, 90, 100) < 0 ||
        SDL_RenderFillRect(renderer, &rect) < 0 ||
        render_copy(renderer, texture, 0, SDL_BLENDMODE_NONE, 255, 255, 255, 255,
                    SDL_FLIP_NONE) < 0 ||
        render_copy(renderer, texture, 20, SDL_BLENDMODE_NONE, 255, 255, 255, 255,
                    SDL_FLIP_HORIZONTAL) < 0 ||
        render_copy(renderer, texture, 40, SDL_BLENDMODE_NONE, 255, 255, 255, 255,
                    SDL_FLIP_VERTICAL) < 0 ||
        render_copy(renderer, texture, 60, SDL_BLENDMODE_NONE, 128, 64, 255, 128,
                    SDL_FLIP_NONE) < 0 ||
        render_copy(renderer, texture, 80, SDL_BLENDMODE_BLEND, 255, 255, 255, 255,
                    SDL_FLIP_NONE) < 0 ||
        render_copy(renderer, texture, 100, SDL_BLENDMODE_ADD, 255, 255, 255, 255,
                    SDL_FLIP_NONE) < 0 ||
        render_copy(renderer, texture, 120, SDL_BLENDMODE_MOD, 255, 255, 255, 255,
                    SDL_FLIP_NONE) < 0 ||
        render_copy(renderer, texture, 140, SDL_BLENDMODE_MUL, 255, 255, 255, 255,
                    SDL_FLIP_NONE) < 0) {
        fprintf(stderr, "standard draw setup: %s\n", SDL_GetError());
        goto done;
    }

    stencil = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE,
                                          SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO,
                                          SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                          SDL_BLENDOPERATION_ADD);
    if (render_copy(renderer, texture, 160, stencil, 255, 255, 255, 255,
                    SDL_FLIP_NONE) < 0) {
        fprintf(stderr, "custom blend setup: %s\n", SDL_GetError());
        goto done;
    }
    rect = (SDL_Rect){ 184, 0, 8, 16 };
    if (SDL_RenderSetClipRect(renderer, &rect) < 0 ||
        render_copy(renderer, texture, 180, SDL_BLENDMODE_NONE, 255, 255, 255, 255,
                    SDL_FLIP_NONE) < 0 ||
        SDL_RenderSetClipRect(renderer, NULL) < 0) {
        fprintf(stderr, "clip setup: %s\n", SDL_GetError());
        goto done;
    }

    /* Force GPU -> CPU synchronization with a scaled copy and software draws. */
    rect = (SDL_Rect){ 0, 32, 32, 32 };
    if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE) < 0 ||
        SDL_SetTextureColorMod(texture, 255, 255, 255) < 0 ||
        SDL_SetTextureAlphaMod(texture, 255) < 0 ||
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) < 0 ||
        SDL_RenderCopy(renderer, texture, NULL, &rect) < 0) {
        fprintf(stderr, "scaled copy setup: %s\n", SDL_GetError());
        goto done;
    }
    rect = (SDL_Rect){ 40, 32, 16, 16 };
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) < 0 ||
        SDL_SetRenderDrawColor(renderer, 100, 50, 200, 128) < 0 ||
        SDL_RenderFillRect(renderer, &rect) < 0) {
        fprintf(stderr, "blended fill setup: %s\n", SDL_GetError());
        goto done;
    }
    rect = (SDL_Rect){ 80, 60, 8, 8 };
    if (SDL_SetRenderDrawBlendMode(renderer, stencil) < 0 ||
        SDL_SetRenderDrawColor(renderer, 220, 80, 40, 128) < 0 ||
        SDL_RenderFillRect(renderer, &rect) < 0) {
        fprintf(stderr, "custom blended fill setup: %s\n", SDL_GetError());
        goto done;
    }
    line[0] = (SDL_Point){ 64, 36 };
    line[1] = (SDL_Point){ 79, 51 };
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE) < 0 ||
        SDL_SetRenderDrawColor(renderer, 9, 199, 111, 77) < 0 ||
        SDL_RenderDrawPoint(renderer, 64, 32) < 0 ||
        SDL_RenderDrawLines(renderer, line, 2) < 0) {
        fprintf(stderr, "point and line setup: %s\n", SDL_GetError());
        goto done;
    }
    triangle[0] = (SDL_Vertex){ { 96.0f, 32.0f }, { 33, 144, 222, 255 }, { 0, 0 } };
    triangle[1] = (SDL_Vertex){ { 120.0f, 32.0f }, { 33, 144, 222, 255 }, { 0, 0 } };
    triangle[2] = (SDL_Vertex){ { 96.0f, 56.0f }, { 33, 144, 222, 255 }, { 0, 0 } };
    if (SDL_RenderGeometry(renderer, NULL, triangle, 3, NULL, 0) < 0) {
        fprintf(stderr, "geometry setup: %s\n", SDL_GetError());
        goto done;
    }

    /* Force CPU -> GPU for an accelerated copy, then GPU -> CPU for a point. */
    rect = (SDL_Rect){ 140, 32, 16, 16 };
    if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE) < 0 ||
        SDL_SetTextureColorMod(texture, 255, 255, 255) < 0 ||
        SDL_SetTextureAlphaMod(texture, 255) < 0 ||
        SDL_RenderCopy(renderer, texture, NULL, &rect) < 0 ||
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE) < 0 ||
        SDL_SetRenderDrawColor(renderer, 231, 17, 99, 211) < 0 ||
        SDL_RenderDrawPoint(renderer, 180, 32) < 0) {
        fprintf(stderr, "mixed synchronization setup: %s\n", SDL_GetError());
        goto done;
    }

    /* A small blended fallback between accelerated draws must synchronize only
       its region without losing pixels elsewhere in the target. */
    rect = (SDL_Rect){ 200, 32, 8, 8 };
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) < 0 ||
        SDL_SetRenderDrawColor(renderer, 180, 60, 20, 128) < 0 ||
        SDL_RenderFillRect(renderer, &rect) < 0) {
        fprintf(stderr, "regional blended fill setup: %s\n", SDL_GetError());
        goto done;
    }
    rect = (SDL_Rect){ 220, 32, 16, 16 };
    if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE) < 0 ||
        SDL_SetTextureColorMod(texture, 255, 255, 255) < 0 ||
        SDL_SetTextureAlphaMod(texture, 255) < 0 ||
        SDL_RenderCopy(renderer, texture, NULL, &rect) < 0) {
        fprintf(stderr, "post-regional hardware copy setup: %s\n", SDL_GetError());
        goto done;
    }

    /* Cross the 64-entry hardware batch boundary, then verify that a fill
       and a following draw remain ordered around the batch flush. */
    {
        SDL_Rect source_rect = { 0, 0, 1, 1 };
        SDL_Rect destination = { 0, 100, 1, 1 };
        for (i = 0; i < 70; ++i) {
            destination.x = i;
            if (SDL_RenderCopy(renderer, texture, &source_rect, &destination) < 0) {
                fprintf(stderr, "batched copy %d setup: %s\n", i, SDL_GetError());
                goto done;
            }
        }
        rect = (SDL_Rect){ 20, 100, 10, 1 };
        if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE) < 0 ||
            SDL_SetRenderDrawColor(renderer, 5, 101, 207, 255) < 0 ||
            SDL_RenderFillRect(renderer, &rect) < 0) {
            fprintf(stderr, "batch ordering fill setup: %s\n", SDL_GetError());
            goto done;
        }
        source_rect.x = 8;
        destination = (SDL_Rect){ 25, 100, 1, 1 };
        if (SDL_RenderCopy(renderer, texture, &source_rect, &destination) < 0) {
            fprintf(stderr, "post-batch copy setup: %s\n", SDL_GetError());
            goto done;
        }
    }

    /* Cross the 64-entry fill-batch boundary, then interleave a draw, a
       second fill run and another draw to verify both ordering boundaries. */
    {
        SDL_Rect fills[70];
        SDL_Rect source_rect = { 0, 0, 1, 1 };
        SDL_Rect destination = { 10, 110, 1, 1 };
        for (i = 0; i < 70; ++i) {
            fills[i] = (SDL_Rect){ i, 110, 1, 1 };
        }
        if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE) < 0 ||
            SDL_SetRenderDrawColor(renderer, 5, 101, 207, 255) < 0 ||
            SDL_RenderFillRects(renderer, fills, 70) < 0 ||
            SDL_RenderCopy(renderer, texture, &source_rect, &destination) < 0) {
            fprintf(stderr, "fill-batch boundary setup: %s\n", SDL_GetError());
            goto done;
        }
        rect = (SDL_Rect){ 20, 110, 10, 1 };
        if (SDL_SetRenderDrawColor(renderer, 193, 47, 83, 255) < 0 ||
            SDL_RenderFillRect(renderer, &rect) < 0) {
            fprintf(stderr, "post-draw fill ordering setup: %s\n", SDL_GetError());
            goto done;
        }
        source_rect.x = 8;
        destination.x = 25;
        if (SDL_RenderCopy(renderer, texture, &source_rect, &destination) < 0) {
            fprintf(stderr, "post-fill draw ordering setup: %s\n", SDL_GetError());
            goto done;
        }
    }

    /* Repeated one-line calls exercise SDL's Noodles-only fill-command merge.
       Verify inclusive endpoints in both directions and every state or draw
       boundary that must stop a merge. */
    {
        SDL_Rect clip = { 30, 120, 3, 1 };
        SDL_Rect viewport = { 60, 120, 16, 2 };
        SDL_Rect source_rect = { 0, 0, 1, 1 };
        SDL_Rect destination = { 45, 120, 1, 1 };
        SDL_Point span[2];
#define DRAW_SPAN(x1, y1, x2, y2) \
        do { \
            span[0] = (SDL_Point){ (x1), (y1) }; \
            span[1] = (SDL_Point){ (x2), (y2) }; \
            if (SDL_RenderDrawLine(renderer, span[0].x, span[0].y, \
                                   span[1].x, span[1].y) < 0) { \
                fprintf(stderr, "merged span setup: %s\n", SDL_GetError()); \
                goto done; \
            } \
        } while (0)
        if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE) < 0 ||
            SDL_SetRenderDrawColor(renderer, 5, 101, 207, 255) < 0) {
            fprintf(stderr, "merged span state setup: %s\n", SDL_GetError());
            goto done;
        }
        DRAW_SPAN(0, 120, 4, 120);
        DRAW_SPAN(14, 120, 10, 120);
        if (SDL_SetRenderDrawColor(renderer, 193, 47, 83, 255) < 0) {
            fprintf(stderr, "merged span color setup: %s\n", SDL_GetError());
            goto done;
        }
        DRAW_SPAN(20, 120, 24, 120);
        if (SDL_SetRenderDrawColor(renderer, 5, 101, 207, 255) < 0 ||
            SDL_RenderSetClipRect(renderer, &clip) < 0) {
            fprintf(stderr, "merged span clip setup: %s\n", SDL_GetError());
            goto done;
        }
        DRAW_SPAN(28, 120, 35, 120);
        if (SDL_RenderSetClipRect(renderer, NULL) < 0) {
            fprintf(stderr, "merged span clip reset: %s\n", SDL_GetError());
            goto done;
        }
        DRAW_SPAN(40, 120, 49, 120);
        if (SDL_RenderCopy(renderer, texture, &source_rect, &destination) < 0) {
            fprintf(stderr, "merged span ordering copy: %s\n", SDL_GetError());
            goto done;
        }
        DRAW_SPAN(47, 120, 52, 120);
        if (SDL_RenderSetViewport(renderer, &viewport) < 0) {
            fprintf(stderr, "merged span viewport setup: %s\n", SDL_GetError());
            goto done;
        }
        DRAW_SPAN(0, 0, 4, 0);
        if (SDL_RenderSetViewport(renderer, NULL) < 0 ||
            SDL_SetRenderDrawColor(renderer, 50, 100, 200, 128) < 0 ||
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE) < 0) {
            fprintf(stderr, "merged span viewport reset: %s\n", SDL_GetError());
            goto done;
        }
        DRAW_SPAN(70, 121, 74, 121);
        if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) < 0) {
            fprintf(stderr, "merged span blend setup: %s\n", SDL_GetError());
            goto done;
        }
        DRAW_SPAN(72, 121, 76, 121);
#undef DRAW_SPAN
    }

    /* Three 4 MiB textures plus the pinned composition exceed the 12 MiB
       test budget. Reusing the first texture verifies eviction restoration. */
    for (i = 0; i < 3; ++i) {
        const uint32_t resident_color = rgba((uint8_t)(40 + i * 70),
                                             (uint8_t)(210 - i * 60),
                                             (uint8_t)(30 + i * 80), 255);
        SDL_Rect update = { 0, 0, 1, 1 };
        SDL_Rect source_rect = { 0, 0, 1, 1 };
        SDL_Rect destination = { i * 4, 80, 1, 1 };
        residency[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STATIC, 1024, 1024);
        if (!residency[i] ||
            SDL_UpdateTexture(residency[i], &update, &resident_color, 4) < 0 ||
            SDL_SetTextureBlendMode(residency[i], SDL_BLENDMODE_NONE) < 0 ||
            SDL_RenderCopy(renderer, residency[i], &source_rect, &destination) < 0) {
            fprintf(stderr, "residency texture %d setup: %s\n", i, SDL_GetError());
            goto done;
        }
    }
    rect = (SDL_Rect){ 12, 80, 1, 1 };
    {
        SDL_Rect source_rect = { 0, 0, 1, 1 };
        if (SDL_RenderCopy(renderer, residency[0], &source_rect, &rect) < 0) {
            fprintf(stderr, "evicted texture restore setup: %s\n", SDL_GetError());
            goto done;
        }
    }

    /* A resident source drops its redundant CPU shadow. Updating it must
       recreate current CPU contents without losing untouched pixels; later
       locks reuse that restored shadow instead of repeating the readback. */
    lazy = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                             SDL_TEXTUREACCESS_STREAMING, 16, 16);
    if (!lazy || SDL_UpdateTexture(lazy, NULL, source, 16 * 4) < 0 ||
        SDL_SetTextureBlendMode(lazy, SDL_BLENDMODE_NONE) < 0) {
        fprintf(stderr, "lazy-shadow texture setup: %s\n", SDL_GetError());
        goto done;
    }
    {
        const uint32_t updated = rgba(17, 93, 201, 255);
        const uint32_t locked = rgba(211, 71, 19, 255);
        SDL_Rect source_rect = { 0, 0, 1, 1 };
        SDL_Rect destination = { 20, 80, 1, 1 };
        void *locked_pixels;
        int locked_pitch;
        if (SDL_RenderCopy(renderer, lazy, &source_rect, &destination) < 0 ||
            SDL_UpdateTexture(lazy, &source_rect, &updated, 4) < 0) {
            fprintf(stderr, "lazy-shadow update transition: %s\n", SDL_GetError());
            goto done;
        }
        destination.x = 24;
        if (SDL_RenderCopy(renderer, lazy, &source_rect, &destination) < 0) {
            fprintf(stderr, "lazy-shadow updated draw: %s\n", SDL_GetError());
            goto done;
        }
        source_rect.x = 1;
        if (SDL_LockTexture(lazy, &source_rect, &locked_pixels, &locked_pitch) < 0) {
            fprintf(stderr, "lazy-shadow lock transition: %s\n", SDL_GetError());
            goto done;
        }
        *(uint32_t *)locked_pixels = locked;
        SDL_UnlockTexture(lazy);
        destination.x = 28;
        if (SDL_RenderCopy(renderer, lazy, &source_rect, &destination) < 0) {
            fprintf(stderr, "lazy-shadow locked draw: %s\n", SDL_GetError());
            goto done;
        }
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ABGR8888,
                             result, WIDTH * 4) < 0) {
        fprintf(stderr, "readback: %s\n", SDL_GetError());
        goto done;
    }

    failures += check_pixel(result, 2, 10, p3, "plain copy") != 0;
    failures += check_pixel(result, 22, 10, p4, "horizontal mirror") != 0;
    failures += check_pixel(result, 42, 10, p1, "vertical mirror") != 0;
    failures += check_pixel(result, 62, 10,
        reference(p3, background, modulation, REF_ONE, REF_ZERO, REF_ADD,
                  REF_ONE, REF_ZERO, REF_ADD, 0), "modulated store") != 0;
    failures += check_pixel(result, 82, 10,
        reference(p3, background, 0xffffffffu, REF_SRC_ALPHA,
                  REF_ONE_MINUS_SRC_ALPHA, REF_ADD, REF_ONE,
                  REF_ONE_MINUS_SRC_ALPHA, REF_ADD, 0), "blend") != 0;
    failures += check_pixel(result, 102, 10,
        reference(p3, background, 0xffffffffu, REF_SRC_ALPHA, REF_ONE, REF_ADD,
                  REF_ZERO, REF_ONE, REF_ADD, 0), "add") != 0;
    failures += check_pixel(result, 122, 10,
        reference(p3, background, 0xffffffffu, REF_DST_COLOR, REF_ZERO, REF_ADD,
                  REF_ZERO, REF_ONE, REF_ADD, 0), "mod") != 0;
    failures += check_pixel(result, 142, 10,
        reference(p3, background, 0xffffffffu, REF_DST_COLOR,
                  REF_ONE_MINUS_SRC_ALPHA, REF_ADD, REF_ZERO, REF_ONE, REF_ADD, 1),
                  "mul") != 0;
    failures += check_pixel(result, 162, 10,
        reference(p3, background, 0xffffffffu, REF_ZERO, REF_ONE, REF_ADD,
                  REF_ZERO, REF_ONE_MINUS_SRC_ALPHA, REF_ADD, 0), "stencil") != 0;
    failures += check_pixel(result, 183, 4, background, "clip outside") != 0;
    failures += check_pixel(result, 184, 4, p1, "clip inside left") != 0;
    failures += check_pixel(result, 191, 4, p2, "clip inside right") != 0;
    failures += check_pixel(result, 225, 5, rgba(70, 80, 90, 100), "opaque fill") != 0;
    failures += check_pixel(result, 4, 52, p3, "scaled copy left") != 0;
    failures += check_pixel(result, 24, 52, p4, "scaled copy right") != 0;
    failures += check_pixel(result, 44, 36, rgba(55, 36, 118, 228),
                            "blended fill") != 0;
    failures += check_pixel(result, 64, 32, rgba(9, 199, 111, 77), "point") != 0;
    failures += check_pixel(result, 72, 44, rgba(9, 199, 111, 77), "line") != 0;
    failures += check_pixel(result, 102, 38, rgba(33, 144, 222, 255),
                            "geometry") != 0;
    failures += check_pixel(result, 142, 42, p3, "post-fallback hardware copy") != 0;
    failures += check_pixel(result, 180, 32, rgba(231, 17, 99, 211),
                            "post-hardware point") != 0;
    failures += check_pixel(result, 203, 35,
        reference(rgba(180, 60, 20, 128), background, 0xffffffffu,
                  REF_SRC_ALPHA, REF_ONE_MINUS_SRC_ALPHA, REF_ADD,
                  REF_ONE, REF_ONE_MINUS_SRC_ALPHA, REF_ADD, 0),
                  "regional blended fill") != 0;
    failures += check_pixel(result, 83, 63,
        reference(rgba(220, 80, 40, 128), background, 0xffffffffu,
                  REF_ZERO, REF_ONE, REF_ADD, REF_ZERO,
                  REF_ONE_MINUS_SRC_ALPHA, REF_ADD, 0),
                  "managed custom blended fill") != 0;
    failures += check_pixel(result, 219, 40, background,
                            "regional boundary") != 0;
    failures += check_pixel(result, 222, 42, p3,
                            "post-regional hardware copy") != 0;
    failures += check_pixel(result, 0, 100, p1, "batch first") != 0;
    failures += check_pixel(result, 19, 100, p1, "batch before fill") != 0;
    failures += check_pixel(result, 20, 100, batch_fill, "batch fill") != 0;
    failures += check_pixel(result, 25, 100, p2, "draw after batch fill") != 0;
    failures += check_pixel(result, 29, 100, batch_fill, "batch fill end") != 0;
    failures += check_pixel(result, 69, 100, p1, "batch last") != 0;
    failures += check_pixel(result, 0, 110, batch_fill, "fill batch first") != 0;
    failures += check_pixel(result, 9, 110, batch_fill, "fill before draw") != 0;
    failures += check_pixel(result, 10, 110, p1, "draw after fill batch") != 0;
    failures += check_pixel(result, 19, 110, batch_fill, "fill batch boundary") != 0;
    failures += check_pixel(result, 20, 110, fill_batch_second,
                            "fill after draw") != 0;
    failures += check_pixel(result, 25, 110, p2, "draw after second fill") != 0;
    failures += check_pixel(result, 29, 110, fill_batch_second,
                            "second fill end") != 0;
    failures += check_pixel(result, 69, 110, batch_fill, "fill batch last") != 0;
    failures += check_pixel(result, 0, 120, batch_fill, "merged line first") != 0;
    failures += check_pixel(result, 4, 120, batch_fill, "merged line inclusive end") != 0;
    failures += check_pixel(result, 5, 120, background, "merged line after end") != 0;
    failures += check_pixel(result, 10, 120, batch_fill, "merged reverse line end") != 0;
    failures += check_pixel(result, 14, 120, batch_fill, "merged reverse line start") != 0;
    failures += check_pixel(result, 20, 120, fill_batch_second, "merged color boundary") != 0;
    failures += check_pixel(result, 29, 120, background, "merged clip outside left") != 0;
    failures += check_pixel(result, 30, 120, batch_fill, "merged clip inside left") != 0;
    failures += check_pixel(result, 32, 120, batch_fill, "merged clip inside right") != 0;
    failures += check_pixel(result, 33, 120, background, "merged clip outside right") != 0;
    failures += check_pixel(result, 45, 120, p1, "merged draw ordering") != 0;
    failures += check_pixel(result, 48, 120, batch_fill, "merged post-draw fill") != 0;
    failures += check_pixel(result, 60, 120, batch_fill, "merged viewport left") != 0;
    failures += check_pixel(result, 64, 120, batch_fill, "merged viewport right") != 0;
    failures += check_pixel(result, 65, 120, background, "merged viewport outside") != 0;
    failures += check_pixel(result, 70, 121, rgba(50, 100, 200, 128),
                            "merged blend initial") != 0;
    failures += check_pixel(result, 72, 121,
        reference(rgba(50, 100, 200, 128), rgba(50, 100, 200, 128),
                  0xffffffffu, REF_SRC_ALPHA, REF_ONE_MINUS_SRC_ALPHA, REF_ADD,
                  REF_ONE, REF_ONE_MINUS_SRC_ALPHA, REF_ADD, 0),
                  "merged blend boundary") != 0;
    failures += check_pixel(result, 76, 121,
        reference(rgba(50, 100, 200, 128), background, 0xffffffffu,
                  REF_SRC_ALPHA, REF_ONE_MINUS_SRC_ALPHA, REF_ADD, REF_ONE,
                  REF_ONE_MINUS_SRC_ALPHA, REF_ADD, 0),
                  "merged blend destination") != 0;
    failures += check_pixel(result, 0, 80, rgba(40, 210, 30, 255),
                            "resident texture zero") != 0;
    failures += check_pixel(result, 4, 80, rgba(110, 150, 110, 255),
                            "resident texture one") != 0;
    failures += check_pixel(result, 8, 80, rgba(180, 90, 190, 255),
                            "resident texture two") != 0;
    failures += check_pixel(result, 12, 80, rgba(40, 210, 30, 255),
                            "evicted texture restore") != 0;
    failures += check_pixel(result, 20, 80, p1,
                            "lazy-shadow initial draw") != 0;
    failures += check_pixel(result, 24, 80, rgba(17, 93, 201, 255),
                            "lazy-shadow update") != 0;
    failures += check_pixel(result, 28, 80, rgba(211, 71, 19, 255),
                            "lazy-shadow lock") != 0;

    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; ++x) {
            hash ^= result[y * WIDTH + x];
            hash *= 16777619u;
        }
    }
    if (failures) {
        fprintf(stderr, "Noodles renderer diagnostic: FAIL (%d checks, hash %08x)\n",
                failures, hash);
        goto done;
    }

    if (draw_display_and_check(renderer, target, p2, "initial") < 0) {
        goto done;
    }
    SDL_ClearError();
    SDL_RenderPresent(renderer);
    if (*SDL_GetError()) {
        fprintf(stderr, "initial display present: %s\n", SDL_GetError());
        goto done;
    }
    /* The first present remains in flight. These commands and their readback
       require the renderer to resolve that fence before using the next back
       buffer, then the second present tests another asynchronous handoff. */
    if (draw_display_and_check(renderer, target, p2, "post-present") < 0) {
        goto done;
    }
    SDL_ClearError();
    SDL_RenderPresent(renderer);
    if (*SDL_GetError()) {
        fprintf(stderr, "second display present: %s\n", SDL_GetError());
        goto done;
    }
    if (audio) {
        Uint32 start = SDL_GetTicks();
        while (SDL_GetQueuedAudioSize(audio) != 0 && SDL_GetTicks() - start < 3000u) {
            SDL_Delay(10);
        }
        if (SDL_GetQueuedAudioSize(audio) != 0) {
            fprintf(stderr, "SDL audio diagnostic: queue did not drain\n");
            goto done;
        }
        SDL_Delay(100);
    }
    printf("Noodles renderer diagnostic: PASS (renderer=%s, hash=%08x)\n", info.name, hash);
    if (audio) {
        printf("SDL audio diagnostic: PASS\n");
    }
    fflush(stdout);
    SDL_Delay(hold_ms);
    rc = 0;

done:
    if (audio) {
        SDL_CloseAudioDevice(audio);
    }
    for (i = 0; i < 3; ++i) {
        SDL_DestroyTexture(residency[i]);
    }
    SDL_DestroyTexture(lazy);
    SDL_DestroyTexture(target);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return rc;
}
