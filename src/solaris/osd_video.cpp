/*
 *   SDL host for SHARP MZ-1500 on Solaris + SDL2.
 * Copyright (c) 2026 M.Yoshiyama
 */

#include "osd.h"
#include "osd_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SOLARIS_SDL_HOST::SOLARIS_SDL_HOST()
    : win_(NULL), surf_(NULL), renderer_(NULL), texture_(NULL),
      texture_pixels_(NULL),
      audio_dev_(0), audio_rate_(0), audio_samples_(0),
      win_w_(0), win_h_(0), render_mode_(0), texture_format_(0), quit_(false)
{
}

SOLARIS_SDL_HOST::~SOLARIS_SDL_HOST()
{
    close();
}


static int find_renderer_driver(const char *name)
{
    int num_drivers = SDL_GetNumRenderDrivers();
    for(int i = 0; i < num_drivers; i++) {
        SDL_RendererInfo info;
        SDL_zero(info);
        if(SDL_GetRenderDriverInfo(i, &info) == 0 &&
           info.name != NULL && strcmp(info.name, name) == 0) {
            return i;
        }
    }
    return -1;
}

bool SOLARIS_SDL_HOST::open_renderer(int w, int h, int render_mode)
{
    const char *driver_name = NULL;
    if(render_mode == 1) {
        driver_name = "opengles2";
    } else if(render_mode == 2) {
        driver_name = "software";
    } else {
        return false;
    }

    int driver_index = find_renderer_driver(driver_name);
    if(driver_index < 0) {
        fprintf(stderr, "SDL renderer '%s' is not available; using window surface\n",
                driver_name);
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    renderer_ = SDL_CreateRenderer(win_, driver_index, 0);
    if(renderer_ == NULL) {
        fprintf(stderr, "SDL_CreateRenderer(%s): %s; using window surface\n",
                driver_name, SDL_GetError());
        return false;
    }

    texture_format_ = SDL_PIXELFORMAT_RGBX8888;
    texture_ = SDL_CreateTexture(renderer_, texture_format_,
                                 SDL_TEXTUREACCESS_STREAMING, w, h);
    if(texture_ == NULL) {
        fprintf(stderr, "SDL_CreateTexture(%s): %s; using window surface\n",
                SDL_GetPixelFormatName(texture_format_), SDL_GetError());
        SDL_DestroyRenderer(renderer_);
        renderer_ = NULL;
        texture_format_ = 0;
        return false;
    }
    texture_pixels_ = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if(texture_pixels_ == NULL) {
        fprintf(stderr, "SDL texture conversion buffer allocation failed; using window surface\n");
        SDL_DestroyTexture(texture_);
        texture_ = NULL;
        SDL_DestroyRenderer(renderer_);
        renderer_ = NULL;
        texture_format_ = 0;
        return false;
    }

    render_mode_ = render_mode;
    return true;
}

bool SOLARIS_SDL_HOST::open(int w, int h, int scale, size_t source_pixel_bytes,
                              int render_mode)
{
    if(scale <= 0) scale = 1;
    win_w_ = w;
    win_h_ = h;
    render_mode_ = 0;

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    win_ = SDL_CreateWindow("SHARP MZ-1500",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            w * scale, h * scale,
                            SDL_WINDOW_SHOWN);
    if(!win_) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    if(open_renderer(w, h, render_mode)) {
        return true;
    }

    surf_ = SDL_GetWindowSurface(win_);
    if(!surf_) {
        fprintf(stderr, "SDL_GetWindowSurface: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void SOLARIS_SDL_HOST::close()
{
    close_audio();
    if(texture_pixels_) { free(texture_pixels_); texture_pixels_ = NULL; }
    if(texture_) { SDL_DestroyTexture(texture_); texture_ = NULL; }
    if(renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = NULL; }
    surf_ = NULL;
    if(win_) { SDL_DestroyWindow(win_); win_ = NULL; }
    SDL_Quit();
}

bool SOLARIS_SDL_HOST::open_audio(int rate, int samples)
{
    if(rate <= 0 || samples <= 0) return false;

    SDL_AudioSpec want;
    SDL_AudioSpec have;
    SDL_zero(want);
    SDL_zero(have);

    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = (Uint16)samples;
    want.callback = NULL;

    audio_dev_ = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                     SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if(audio_dev_ == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return false;
    }
    if(have.format != AUDIO_S16SYS || have.channels != 2) {
        fprintf(stderr, "Unsupported audio format: format=0x%x channels=%u\n",
                (unsigned)have.format, (unsigned)have.channels);
        close_audio();
        return false;
    }

    audio_rate_ = have.freq;
    audio_samples_ = samples;
    SDL_PauseAudioDevice(audio_dev_, 0);
    return true;
}

void SOLARIS_SDL_HOST::close_audio()
{
    if(audio_dev_ != 0) {
        SDL_ClearQueuedAudio(audio_dev_);
        SDL_CloseAudioDevice(audio_dev_);
        audio_dev_ = 0;
    }
    audio_rate_ = 0;
    audio_samples_ = 0;
}

bool SOLARIS_SDL_HOST::queue_audio(const void *samples, int frames)
{
    if(audio_dev_ == 0 || samples == NULL || frames <= 0) return false;

    const uint32_t bytes = (uint32_t)frames * 2u * (uint32_t)sizeof(int16_t);
    if(SDL_QueueAudio(audio_dev_, samples, bytes) != 0) {
        fprintf(stderr, "SDL_QueueAudio: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

uint32_t SOLARIS_SDL_HOST::queued_audio_bytes() const
{
    if(audio_dev_ == 0) return 0;
    return SDL_GetQueuedAudioSize(audio_dev_);
}

int SOLARIS_SDL_HOST::sdl_to_vk(SDL_Keycode k) const
{
    if(k >= SDLK_a && k <= SDLK_z) return VK_A + (k - SDLK_a);
    if(k >= SDLK_0 && k <= SDLK_9) return VK_0 + (k - SDLK_0);

    switch(k) {
    case SDLK_BACKSPACE: return VK_BACK;
    case SDLK_TAB: return VK_TAB;
    case SDLK_RETURN: return VK_RETURN;
    case SDLK_ESCAPE: return VK_ESCAPE;
    case SDLK_SPACE: return VK_SPACE;
    case SDLK_PAGEUP: return VK_PRIOR;
    case SDLK_PAGEDOWN: return VK_NEXT;
    case SDLK_END: return VK_END;
    case SDLK_HOME: return VK_HOME;
    case SDLK_LEFT: return VK_LEFT;
    case SDLK_UP: return VK_UP;
    case SDLK_RIGHT: return VK_RIGHT;
    case SDLK_DOWN: return VK_DOWN;
    case SDLK_INSERT: return VK_INSERT;
    case SDLK_DELETE: return VK_DELETE;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT: return VK_SHIFT;
    case SDLK_LCTRL:
    case SDLK_RCTRL: return VK_CONTROL;
    case SDLK_LALT:
    case SDLK_RALT: return VK_MENU;
    case SDLK_F1: return VK_F1;
    case SDLK_F2: return VK_F2;
    case SDLK_F3: return VK_F3;
    case SDLK_F4: return VK_F4;
    case SDLK_F5: return VK_F5;
    case SDLK_F6: return VK_F6;
    case SDLK_F7: return VK_F7;
    case SDLK_F8: return VK_F8;
    case SDLK_F9: return VK_F9;
    case SDLK_F10: return VK_F10;
    case SDLK_F11: return VK_F11;
    case SDLK_F12: return VK_F12;
    case SDLK_SEMICOLON: return VK_OEM_1;
    case SDLK_EQUALS: return VK_OEM_PLUS;
    case SDLK_COMMA: return VK_OEM_COMMA;
    case SDLK_MINUS: return VK_OEM_MINUS;
    case SDLK_PERIOD: return VK_OEM_PERIOD;
    case SDLK_SLASH: return VK_OEM_2;
    case SDLK_BACKQUOTE: return VK_OEM_3;
    case SDLK_LEFTBRACKET: return VK_OEM_4;
    case SDLK_BACKSLASH: return VK_OEM_5;
    case SDLK_RIGHTBRACKET: return VK_OEM_6;
    case SDLK_QUOTE: return VK_OEM_7;
    default: return 0;
    }
}

bool SOLARIS_SDL_HOST::poll(uint8_t key_status[256], bool *reset_requested)
{
    if(reset_requested) *reset_requested = false;

    SDL_Event e;
    while(SDL_PollEvent(&e)) {
        if(e.type == SDL_QUIT) {
            quit_ = true;
        } else if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            int vk = sdl_to_vk(e.key.keysym.sym);
            if(vk == VK_F12 && e.type == SDL_KEYDOWN) {
                if(reset_requested) *reset_requested = true;
                continue;
            }
            if(vk >= 0 && vk < 256) {
                key_status[vk] = (e.type == SDL_KEYDOWN) ? 0x80 : 0x00;
            }
        }
    }
    return !quit_;
}

void SOLARIS_SDL_HOST::present(const void *pixels, int w, int h, int pitch_bytes)
{
    if(!win_ || !pixels || w <= 0 || h <= 0 || pitch_bytes <= 0) return;

    if(renderer_ != NULL && texture_ != NULL) {
        if(texture_pixels_ == NULL) return;

        const uint8_t *src_line = (const uint8_t *)pixels;
        for(int y = 0; y < h; y++) {
            const uint32_t *src = (const uint32_t *)(const void *)(src_line + y * pitch_bytes);
            uint32_t *dst = texture_pixels_ + (size_t)y * (size_t)w;
            for(int x = 0; x < w; x++) {
                dst[x] = (src[x] << 8) | 0xff;
            }
        }

        if(SDL_UpdateTexture(texture_, NULL, texture_pixels_, w * (int)sizeof(uint32_t)) != 0) {
            fprintf(stderr, "SDL_UpdateTexture: %s\n", SDL_GetError());
            return;
        }
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, NULL, NULL);
        SDL_RenderPresent(renderer_);
        return;
    }

    if(!surf_ || !surf_->pixels) return;

    const bool must_lock = SDL_MUSTLOCK(surf_) != 0;
    if(must_lock) {
        if(SDL_LockSurface(surf_) != 0) return;
    }

    const uint8_t *src = (const uint8_t *)pixels;
    uint8_t *dst = (uint8_t *)surf_->pixels;
    int copy_w = w * (int)sizeof(uint32_t);
    if(copy_w > pitch_bytes) copy_w = pitch_bytes;
    if(copy_w > surf_->pitch) copy_w = surf_->pitch;
    int copy_h = h;
    if(copy_h > surf_->h) copy_h = surf_->h;

    if(copy_w == pitch_bytes && copy_w == surf_->pitch) {
        memcpy(dst, src, (size_t)copy_w * (size_t)copy_h);
    } else {
        for(int y = 0; y < copy_h; y++) {
            memcpy(dst + y * surf_->pitch, src + y * pitch_bytes, copy_w);
        }
    }

    if(must_lock) {
        SDL_UnlockSurface(surf_);
    }
    SDL_UpdateWindowSurface(win_);
}

void SOLARIS_SDL_HOST::delay_ms(unsigned ms)
{
    SDL_Delay(ms);
}
