/*
 * Direct VM host for SHARP MZ-1500 on Solaris + SDL2.
 */

#include "compat.h"
#include "sdl_host.h"

#include "../common.h"
#include "../config.h"
#include "../emu.h"
#include "../vm/vm.h"
#include "../vm/vm_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <mutex>
#include <queue>
#include <string>

extern config_t config;

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [tape-file]\n"
        "       %s --cmt tape-file\n"
        "       %s --qd quick-disk-file\n"
        "  F12: reset\n"
        "  Console: help, status, qd <file>, cmt <file>, cmtplay, cmtrec <file>, cmtstop, reset, exit\n"
        "  Window close: quit\n",
        argv0, argv0, argv0);
}

struct ConsoleState {
    std::mutex mutex;
    std::queue<std::string> commands;
};

static int positive_or(int value, int fallback)
{
    return (value > 0) ? value : fallback;
}

static int sound_frequency_to_rate(int frequency)
{
    static const int table[8] = {
        2000, 4000, 8000, 11025, 22050, 44100, 48000, 96000
    };
    if(frequency < 0 || frequency >= 8) return 44100;
    return table[frequency];
}

struct DirectConfig {
    int render_mode;
    int sound_rate;
    int sound_samples;
    int frame_skip;
    int screen_scale;
    int audio_target_chunks;
    int audio_initial_chunks;
    int audio_max_refill_chunks;
    std::string initial_cmt;
    std::string initial_qd;

    DirectConfig()
        : render_mode(config.solaris_render_mode),
          sound_rate(sound_frequency_to_rate(config.sound_frequency)),
          sound_samples(positive_or(config.solaris_sound_samples, 512)),
          frame_skip(positive_or(config.solaris_frame_skip, 8)),
          screen_scale(positive_or(config.solaris_screen_scale, 1)),
          audio_target_chunks(positive_or(config.solaris_audio_target_chunks, 4)),
          audio_initial_chunks(positive_or(config.solaris_audio_initial_chunks, 3)),
          audio_max_refill_chunks(positive_or(config.solaris_audio_max_refill_chunks, 1)),
          initial_cmt(config.solaris_initial_cmt),
          initial_qd(config.solaris_initial_qd)
    {
        if(render_mode < 0) render_mode = 0;
        if(render_mode > 2) render_mode = 2;
    }
};

static std::string trim_string(const std::string& s)
{
    size_t first = s.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return std::string();
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static std::string strip_quotes(const std::string& s)
{
    if(s.size() >= 2) {
        char first = s[0];
        char last = s[s.size() - 1];
        if((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

static void print_console_help()
{
    fprintf(stderr,
        "Console commands:\n"
        "  help\n"
        "  status\n"
        "  qd <quick-disk-file>\n"
        "  cmt <tape-file>\n"
        "  cmtplay\n"
        "  cmtrec <tape-file>\n"
        "  cmtstop\n"
        "  reset\n"
        "  exit\n");
}

static int SDLCALL console_thread_proc(void *data)
{
    ConsoleState *state = (ConsoleState *)data;
    char line[1024];

    print_console_help();
    while(fgets(line, sizeof(line), stdin) != NULL) {
        std::string command = trim_string(line);
        if(command.empty()) continue;

        std::lock_guard<std::mutex> lock(state->mutex);
        state->commands.push(command);
    }
    return 0;
}

static bool file_exists(const char *path)
{
    if(path == NULL || path[0] == '\0') return false;

    struct stat st;
    if(stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static void queue_sound_chunks(EMU& emu, SOLARIS_SDL_HOST& host,
                               int sound_samples, uint32_t audio_chunk_bytes,
                               int target_chunks, int max_chunks)
{
    int queued_chunks = (int)(host.queued_audio_bytes() / audio_chunk_bytes);
    int chunks = target_chunks - queued_chunks;
    if(chunks > max_chunks) chunks = max_chunks;

    for(int i = 0; i < chunks; i++) {
        int extra_frames = 0;
        uint16_t *sound = emu.get_vm()->create_sound(&extra_frames);
        if(sound == NULL) break;
        if(!host.queue_audio(sound, sound_samples)) break;
    }
}

static bool pop_console_command(ConsoleState *state, std::string *command)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if(state->commands.empty()) return false;

    *command = state->commands.front();
    state->commands.pop();
    return true;
}

static void print_status(EMU& emu, SOLARIS_SDL_HOST& host,
                         const DirectConfig& direct_config, bool audio_enabled)
{
    fprintf(stderr,
            "Status:\n"
            "  screen: %dx%d scale=%d frame_skip=%d\n"
            "  renderer: mode=%d\n"
            "  audio: %s rate=%d samples=%d queued=%u bytes\n"
            "  audio_buffer: target=%d initial=%d max_refill=%d\n",
            emu.screen_width(), emu.screen_height(),
            direct_config.screen_scale, direct_config.frame_skip,
            host.render_mode(),
            audio_enabled && host.audio_opened() ? "on" : "off",
            host.audio_rate(), host.audio_samples(),
            (unsigned)host.queued_audio_bytes(),
            direct_config.audio_target_chunks,
            direct_config.audio_initial_chunks,
            direct_config.audio_max_refill_chunks);
}
// Process console commands from the queue. This is called from the main thread, so it can safely interact with the VM and host.
static void process_console_commands(ConsoleState *state, EMU& emu, SOLARIS_SDL_HOST& host, const DirectConfig& direct_config, bool audio_enabled, bool *exit_requested)
{
    std::string line;
    while(pop_console_command(state, &line)) {
        std::string cmd;
        std::string arg;
        size_t sep = line.find_first_of(" \t");
        if(sep == std::string::npos) {
            cmd = line;
        } else {
            cmd = line.substr(0, sep);
            arg = strip_quotes(trim_string(line.substr(sep + 1)));
        }

        if(cmd == "help") {
            print_console_help();
        } else if(cmd == "status") {
            print_status(emu, host, direct_config, audio_enabled);
        } else if(cmd == "exit") {
            *exit_requested = true;
        } else if(cmd == "reset") {
            emu.get_vm()->reset();
            fprintf(stderr, "VM reset\n");
        } else if(cmd == "qd") {
            if(arg.empty()) {
                fprintf(stderr, "Usage: qd <quick-disk-file>\n");
            } else if(!file_exists(arg.c_str())) {
                fprintf(stderr, "QuickDisk file not found: %s\n", arg.c_str());
            } else {
                emu.get_vm()->open_quick_disk(0, arg.c_str());
                fprintf(stderr, "QuickDisk mounted: %s\n", arg.c_str());
            }
        } else if(cmd == "cmt" || cmd == "--cmt") {
            if(arg.empty()) {
                fprintf(stderr, "Usage: cmt <tape-file>\n");
            } else if(!file_exists(arg.c_str())) {
                fprintf(stderr, "CMT file not found: %s\n", arg.c_str());
            } else {
                emu.get_vm()->push_stop(0);
                emu.get_vm()->play_tape(0, arg.c_str());
                fprintf(stderr, "CMT mounted: %s\n", arg.c_str());
            }
        } else if(cmd == "cmtplay") {
            emu.get_vm()->push_play(0);
            fprintf(stderr, "CMT play\n");
        } else if(cmd == "cmtrec") {
            if(arg.empty()) {
                fprintf(stderr, "Usage: cmtrec <tape-file>\n");
            } else {
                emu.get_vm()->push_stop(0);
                emu.get_vm()->rec_tape(0, arg.c_str());
                emu.get_vm()->push_play(0);
                fprintf(stderr, "CMT rec: %s\n", arg.c_str());
            }
        } else if(cmd == "cmtstop") {
            emu.get_vm()->push_stop(0);
            fprintf(stderr, "CMT stop\n");
        } else {
            fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
            fprintf(stderr, "Type 'help' for commands.\n");
        }
    }
}

int main(int argc, char **argv)
{
    if(argc > 3) {
        usage(argv[0]);
        return 1;
    }

    load_config(create_local_path(_T("%s.ini"), _T(CONFIG_NAME)));
    DirectConfig direct_config;

    EMU emu;
    VM *vm = new VM(&emu);
    if(vm == NULL) {
        fprintf(stderr, "VM init failed\n");
        return 1;
    }
    emu.set_vm(vm);
    vm->initialize();
    vm->reset();

    if(argc == 2) {
        if(strcmp(argv[1], "--cmt") == 0 || strcmp(argv[1], "--qd") == 0) {
            usage(argv[0]);
            delete vm;
            return 1;
        }
        if(!file_exists(argv[1])) {
            fprintf(stderr, "CMT file not found: %s\n", argv[1]);
            delete vm;
            return 1;
        }
        direct_config.initial_cmt = argv[1];
        direct_config.initial_qd.clear();
    } else if(argc == 3) {
        if(strcmp(argv[1], "--cmt") == 0) {
            if(!file_exists(argv[2])) {
                fprintf(stderr, "CMT file not found: %s\n", argv[2]);
                delete vm;
                return 1;
            }
            direct_config.initial_cmt = argv[2];
            direct_config.initial_qd.clear();
        } else if(strcmp(argv[1], "--qd") == 0) {
            if(!file_exists(argv[2])) {
                fprintf(stderr, "QuickDisk file not found: %s\n", argv[2]);
                delete vm;
                return 1;
            }
            direct_config.initial_qd = argv[2];
            direct_config.initial_cmt.clear();
        } else {
            usage(argv[0]);
            delete vm;
            return 1;
        }
    }

    if(!direct_config.initial_cmt.empty()) {
        if(!file_exists(direct_config.initial_cmt.c_str())) {
            fprintf(stderr, "CMT file not found: %s\n", direct_config.initial_cmt.c_str());
            delete vm;
            return 1;
        }
        vm->play_tape(0, direct_config.initial_cmt.c_str());
        vm->push_play(0);
        fprintf(stderr, "CMT mounted: %s\n", direct_config.initial_cmt.c_str());
    }
    if(!direct_config.initial_qd.empty()) {
        if(!file_exists(direct_config.initial_qd.c_str())) {
            fprintf(stderr, "QuickDisk file not found: %s\n", direct_config.initial_qd.c_str());
            delete vm;
            return 1;
        }
        vm->open_quick_disk(0, direct_config.initial_qd.c_str());
        fprintf(stderr, "QuickDisk mounted: %s\n", direct_config.initial_qd.c_str());
    }

    SOLARIS_SDL_HOST host;
    if(!host.open(emu.screen_width(), emu.screen_height(), direct_config.screen_scale,
                  sizeof(scrntype_t), direct_config.render_mode)) {
        delete vm;
        return 1;
    }

    ConsoleState *console_state = new ConsoleState;
    SDL_Thread *console_thread = SDL_CreateThread(console_thread_proc, "console", console_state);
    if(console_thread != NULL) {
        SDL_DetachThread(console_thread);
    } else {
        fprintf(stderr, "SDL_CreateThread(console): %s\n", SDL_GetError());
    }

    const int sound_samples = direct_config.sound_samples;
    bool audio_enabled = host.open_audio(direct_config.sound_rate, sound_samples);
    if(audio_enabled) {
        emu.set_sound_rate(host.audio_rate());
        vm->initialize_sound(host.audio_rate(), sound_samples);
    }

    const int screen_w = emu.screen_width();
    const int screen_h = emu.screen_height();
    const int screen_pitch = screen_w * (int)sizeof(scrntype_t);
    const uint32_t audio_chunk_bytes =
        (uint32_t)sound_samples * 2u * (uint32_t)sizeof(uint16_t);
    const int audio_target_chunks = direct_config.audio_target_chunks;
    const int audio_initial_chunks = direct_config.audio_initial_chunks;
    const int audio_max_refill_chunks = direct_config.audio_max_refill_chunks;
    if(audio_enabled) {
        queue_sound_chunks(emu, host, sound_samples, audio_chunk_bytes,
                           audio_initial_chunks, audio_initial_chunks);
    }

    const double fps = emu.get_frame_rate();
    const unsigned frame_ms = (fps > 1.0) ? (unsigned)(500.0 / fps + 0.5) : 8;

    int frame_count = 0;
    int frame_skip = direct_config.frame_skip;

    bool exit_requested = false;
    while(!host.quit_requested() && !exit_requested) {
        bool reset = false;

        host.poll(emu.key_buffer(), &reset);

        if(reset) {
            vm->reset();
        }

        emu.lock_vm();
        vm->run();

        process_console_commands(console_state, emu, host, direct_config, audio_enabled, &exit_requested);

        if(audio_enabled && host.queued_audio_bytes() < audio_chunk_bytes * (uint32_t)audio_target_chunks) {
            queue_sound_chunks(emu, host, sound_samples, audio_chunk_bytes,
                               audio_target_chunks, audio_max_refill_chunks);
        }

        // frame skipping: only present every N frames to limit CPU usage. The VM runs at full speed regardless.
        if((frame_count++ % frame_skip) == 0) {
            vm->draw_screen();
            host.present(emu.screen_buffer(), screen_w, screen_h, screen_pitch);
        }

        emu.unlock_vm();

        host.delay_ms(frame_ms);
    }

    delete console_state;
    delete vm;
    emu.set_vm(NULL);
    return 0;
}