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

int main(int argc, char **argv)
{
    const uint32_t background = rgba(11, 23, 37, 201);
    const uint32_t p1 = rgba(200, 10, 20, 255);
    const uint32_t p2 = rgba(20, 210, 30, 192);
    const uint32_t p3 = rgba(30, 40, 220, 128);
    const uint32_t p4 = rgba(240, 230, 50, 64);
    const uint32_t modulation = rgba(128, 64, 255, 128);
    uint32_t source[16 * 16];
    uint32_t result[WIDTH * HEIGHT];
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_Texture *target = NULL;
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

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        goto done;
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
    failures += check_pixel(result, 0, 80, rgba(40, 210, 30, 255),
                            "resident texture zero") != 0;
    failures += check_pixel(result, 4, 80, rgba(110, 150, 110, 255),
                            "resident texture one") != 0;
    failures += check_pixel(result, 8, 80, rgba(180, 90, 190, 255),
                            "resident texture two") != 0;
    failures += check_pixel(result, 12, 80, rgba(40, 210, 30, 255),
                            "evicted texture restore") != 0;

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

    if (SDL_SetRenderTarget(renderer, NULL) < 0 ||
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) < 0 ||
        SDL_RenderClear(renderer) < 0) {
        fprintf(stderr, "display clear: %s\n", SDL_GetError());
        goto done;
    }
    rect = (SDL_Rect){ (800 - WIDTH) / 2, (600 - HEIGHT) / 2, WIDTH, HEIGHT };
    if (SDL_SetTextureBlendMode(target, SDL_BLENDMODE_NONE) < 0 ||
        SDL_SetTextureColorMod(target, 255, 255, 255) < 0 ||
        SDL_SetTextureAlphaMod(target, 255) < 0 ||
        SDL_RenderCopy(renderer, target, NULL, &rect) < 0) {
        fprintf(stderr, "display copy: %s\n", SDL_GetError());
        goto done;
    }
    SDL_ClearError();
    SDL_RenderPresent(renderer);
    if (*SDL_GetError()) {
        fprintf(stderr, "display present: %s\n", SDL_GetError());
        goto done;
    }
    printf("Noodles renderer diagnostic: PASS (renderer=%s, hash=%08x)\n", info.name, hash);
    fflush(stdout);
    SDL_Delay(hold_ms);
    rc = 0;

done:
    for (i = 0; i < 3; ++i) {
        SDL_DestroyTexture(residency[i]);
    }
    SDL_DestroyTexture(target);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return rc;
}
