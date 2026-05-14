#pragma once

#include <SDL.h>
#include <stdint.h>
#include <stddef.h>

class SOLARIS_SDL_HOST {
public:
    SOLARIS_SDL_HOST();
    ~SOLARIS_SDL_HOST();

    bool open(int w, int h, int scale, size_t source_pixel_bytes, int render_mode);
    void close();

    bool poll(uint8_t key_status[256], bool *reset_requested);
    void present(const void *pixels, int w, int h, int pitch_bytes);
    bool open_audio(int rate, int samples);
    void close_audio();
    bool queue_audio(const void *samples, int frames);
    uint32_t queued_audio_bytes() const;
    bool audio_opened() const { return audio_dev_ != 0; }
    int audio_rate() const { return audio_rate_; }
    int audio_samples() const { return audio_samples_; }
    int render_mode() const { return render_mode_; }
    void delay_ms(unsigned ms);
    bool quit_requested() const { return quit_; }

private:
    SDL_Window *win_;
    SDL_Surface *surf_;
    SDL_Renderer *renderer_;
    SDL_Texture *texture_;
    uint32_t *texture_pixels_;
    SDL_AudioDeviceID audio_dev_;
    int audio_rate_;
    int audio_samples_;
    int win_w_;
    int win_h_;
    int render_mode_;
    uint32_t texture_format_;
    bool quit_;

    int sdl_to_vk(SDL_Keycode k) const;
    bool open_renderer(int w, int h, int render_mode);
};
