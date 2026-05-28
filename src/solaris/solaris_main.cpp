/*
 * Direct VM host for SHARP MZ-1500 on Solaris + SDL2.
 * Copyright (c) 2026 M.Yoshiyama
 */

#include "osd_compat.h"
#include "osd.h"
#include "osd_console.h"

#include "../common.h"
#include "../config.h"
#include "../emu.h"
#include "../vm/vm.h"
#include "../vm/vm_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
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
#ifdef DEBUG
        "  Console: help, status, qd <file>, qdeject, cmt <file>, cmtplay, cmtrec <file>, cmtstop, cmteject, cmtff, cmtrew, option <mask> <0|1>, frequency <0-7>, latency <0-4>, frameskip <1|2|4|8>, reset, exit\n"
#endif
        "  Window close: quit\n",
        argv0, argv0, argv0);
}

#ifdef DEBUG
struct ConsoleState {
    std::mutex mutex;
    std::queue<std::string> commands;
};
#endif

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

static bool is_valid_frame_skip(long frame_skip)
{
    return frame_skip == 1 || frame_skip == 2 || frame_skip == 4 || frame_skip == 8;
}

static int normalize_frame_skip(int frame_skip)
{
    return is_valid_frame_skip(frame_skip) ? frame_skip : 1;
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
          frame_skip(normalize_frame_skip(config.solaris_frame_skip)),
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
            std::string unquoted;
            for(size_t i = 1; i + 1 < s.size(); i++) {
                if(s[i] == '\\' && i + 2 < s.size()) {
                    char next = s[i + 1];
                    if(next == first || next == '\\') {
                        unquoted += next;
                        i++;
                        continue;
                    }
                }
                unquoted += s[i];
            }
            return unquoted;
        }
    }
    return s;
}

#ifdef DEBUG
static void print_console_help()
{
    fprintf(stderr,
        "Console commands:\n"
        "  help\n"
        "  status\n"
        "  qd <quick-disk-file>\n"
        "  qdeject\n"
        "  cmt <tape-file>\n"
        "  cmtplay\n"
        "  cmtrec <tape-file>\n"
        "  cmtstop\n"
        "  cmteject\n"
        "  cmtff\n"
        "  cmtrew\n"
        "  option <mask> <0|1>\n"
        "  frequency <0-7>\n"
        "  latency <0-4>\n"
        "  frameskip <1|2|4|8>\n"
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
#endif

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

#ifdef DEBUG
static bool pop_console_command(ConsoleState *state, std::string *command)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if(state->commands.empty()) return false;

    *command = state->commands.front();
    state->commands.pop();
    return true;
}
#endif

static void save_current_config()
{
    save_config(create_local_path(_T("%s.ini"), _T(CONFIG_NAME)));
}

static bool reset_vm(EMU& emu, VM *&vm, bool audio_enabled, int sound_rate, int sound_samples)
{
    memset(emu.key_buffer(), 0, 256);
    delete vm;
    vm = new VM(&emu);
    if(vm == NULL) {
        emu.set_vm(NULL);
        return false;
    }
    emu.set_vm(vm);
    vm->initialize();
    if(audio_enabled) {
        emu.set_sound_rate(sound_rate);
        vm->initialize_sound(sound_rate, sound_samples);
    }
    vm->reset();
    return true;
}

#ifdef DEBUG
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
#endif
static void process_command_line(const std::string& line, EMU& emu, SOLARIS_SDL_HOST& host, DirectConfig& direct_config, bool audio_enabled, bool *exit_requested, bool *reset_requested)
{
    std::string cmd;
    std::string arg;
    size_t sep = line.find_first_of(" \t");

    if(sep == std::string::npos) {
        cmd = line;
    } else {
        cmd = line.substr(0, sep);
        arg = strip_quotes(trim_string(line.substr(sep + 1)));
    }

#ifdef DEBUG
    if(cmd == "help") {
        print_console_help();
    } else if(cmd == "status") {
        print_status(emu, host, direct_config, audio_enabled);
    } else
#endif
    if(cmd == "exit") {
        *exit_requested = true;
    } else if(cmd == "reset") {
        *reset_requested = true;
#ifdef DEBUG
        fprintf(stderr, "VM reset\n");
#endif
    } else if(cmd == "qd") {
        if(arg.empty()) {
            fprintf(stderr, "Usage: qd <quick-disk-file>\n");
        } else if(!file_exists(arg.c_str())) {
            fprintf(stderr, "QuickDisk file not found: %s\n", arg.c_str());
        } else {
            emu.get_vm()->open_quick_disk(0, arg.c_str());
#ifdef DEBUG
            fprintf(stderr, "QuickDisk mounted: %s\n", arg.c_str());
#endif
        }
    } else if(cmd == "cmt" || cmd == "--cmt") {
        if(arg.empty()) {
            fprintf(stderr, "Usage: cmt <tape-file>\n");
        } else if(!file_exists(arg.c_str())) {
            fprintf(stderr, "CMT file not found: %s\n", arg.c_str());
        } else {
            emu.get_vm()->push_stop(0);
            emu.get_vm()->play_tape(0, arg.c_str());
#ifdef DEBUG
            fprintf(stderr, "CMT mounted: %s\n", arg.c_str());
#endif
        }
    } else if(cmd == "cmtplay") {
        emu.get_vm()->push_play(0);
#ifdef DEBUG
        fprintf(stderr, "CMT play\n");
#endif
    } else if(cmd == "cmtrec") {
        if(arg.empty()) {
            fprintf(stderr, "Usage: cmtrec <tape-file>\n");
        } else {
            emu.get_vm()->push_stop(0);
            emu.get_vm()->rec_tape(0, arg.c_str());
            emu.get_vm()->push_play(0);
#ifdef DEBUG
            fprintf(stderr, "CMT rec: %s\n", arg.c_str());
#endif
        }
    } else if(cmd == "cmtstop") {
        emu.get_vm()->push_stop(0);
#ifdef DEBUG
        fprintf(stderr, "CMT stop\n");
#endif
    } else if(cmd == "cmteject") {
        emu.get_vm()->close_tape(0);
#ifdef DEBUG
        fprintf(stderr, "CMT ejected\n");
#endif
    } else if(cmd == "cmtff") {
        emu.get_vm()->push_fast_forward(0);
#ifdef DEBUG
        fprintf(stderr, "CMT fast forward\n");
#endif
    } else if(cmd == "cmtrew") {
        emu.get_vm()->push_fast_rewind(0);
#ifdef DEBUG
        fprintf(stderr, "CMT fast rewind\n");
#endif
    } else if(cmd == "qdeject") {
        emu.get_vm()->close_quick_disk(0);
#ifdef DEBUG
        fprintf(stderr, "QuickDisk ejected\n");
#endif
    } else if(cmd == "option") {
        if(arg.empty()) {
            fprintf(stderr, "Usage: option <mask> <0|1>\n");
        } else {
            char *end = NULL;
            unsigned long mask = strtoul(arg.c_str(), &end, 0);
            while(end != NULL && (*end == ' ' || *end == '\t')) end++;
            int enabled = (end != NULL) ? atoi(end) : 0;
            if(mask == 0) {
                fprintf(stderr, "Invalid option mask: %s\n", arg.c_str());
            } else {
                if(enabled) {
                    config.option_switch |= (uint32_t)mask;
                } else {
                    config.option_switch &= ~(uint32_t)mask;
                }
                emu.get_vm()->update_config();
#ifdef DEBUG
                fprintf(stderr, "Option switch: mask=0x%lx %s\n", mask, enabled ? "on" : "off");
#endif
            }
        }
    } else if(cmd == "frequency") {
        char *end = NULL;
        long value = strtol(arg.c_str(), &end, 0);
        if(arg.empty() || end == arg.c_str() || value < 0 || value > 7) {
            fprintf(stderr, "Usage: frequency <0-7>\n");
        } else {
            config.sound_frequency = (int)value;
            save_current_config();
#ifdef DEBUG
            fprintf(stderr, "Frequency set to %ld (applied on next launch)\n", value);
#endif
        }
    } else if(cmd == "latency") {
        char *end = NULL;
        long value = strtol(arg.c_str(), &end, 0);
        if(arg.empty() || end == arg.c_str() || value < 0 || value > 4) {
            fprintf(stderr, "Usage: latency <0-4>\n");
        } else {
            config.sound_latency = (int)value;
            save_current_config();
#ifdef DEBUG
            fprintf(stderr, "Latency set to %ld (applied on next launch)\n", value);
#endif
        }
    } else if(cmd == "frameskip") {
        char *end = NULL;
        long value = strtol(arg.c_str(), &end, 0);
        if(arg.empty() || end == arg.c_str() || !is_valid_frame_skip(value)) {
            fprintf(stderr, "Usage: frameskip <1|2|4|8>\n");
        } else {
            config.solaris_frame_skip = (int)value;
            direct_config.frame_skip = (int)value;
            save_current_config();
#ifdef DEBUG
            fprintf(stderr, "Frame skip set to %ld\n", value);
#endif
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
        fprintf(stderr, "Type 'help' for commands.\n");
    }
}

#ifdef DEBUG
static void process_console_commands(ConsoleState *state, EMU& emu, SOLARIS_SDL_HOST& host, DirectConfig& direct_config, bool audio_enabled, bool *exit_requested, bool *reset_requested)
{
    std::string line;
    while(pop_console_command(state, &line)) {
        process_command_line(line, emu, host, direct_config, audio_enabled, exit_requested, reset_requested);
    }
}
#endif

static void process_control_commands(SOLARIS_CONTROL_WINDOW *control_window, EMU& emu, SOLARIS_SDL_HOST& host, DirectConfig& direct_config, bool audio_enabled, bool *exit_requested, bool *reset_requested)
{
    std::string line;
    while(control_window->pop_command(&line)) {
        process_command_line(line, emu, host, direct_config, audio_enabled, exit_requested, reset_requested);
    }
}

int main(int argc, char **argv)
{
    XInitThreads();

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
#ifdef DEBUG
        fprintf(stderr, "CMT mounted: %s\n", direct_config.initial_cmt.c_str());
#endif
    }
    if(!direct_config.initial_qd.empty()) {
        if(!file_exists(direct_config.initial_qd.c_str())) {
            fprintf(stderr, "QuickDisk file not found: %s\n", direct_config.initial_qd.c_str());
            delete vm;
            return 1;
        }
        vm->open_quick_disk(0, direct_config.initial_qd.c_str());
#ifdef DEBUG
        fprintf(stderr, "QuickDisk mounted: %s\n", direct_config.initial_qd.c_str());
#endif
    }

    SOLARIS_SDL_HOST host;
    if(!host.open(emu.screen_width(), emu.screen_height(), direct_config.screen_scale,
                  sizeof(scrntype_t), direct_config.render_mode)) {
        delete vm;
        return 1;
    }

#ifdef DEBUG
    // The detached console thread can remain blocked in fgets() until process exit.
    // Keep this state alive for the process lifetime to avoid a stale queue pointer.
    ConsoleState *console_state = new ConsoleState;
    SDL_Thread *console_thread = SDL_CreateThread(console_thread_proc, "console", console_state);
    if(console_thread != NULL) {
        SDL_DetachThread(console_thread);
    } else {
        fprintf(stderr, "SDL_CreateThread(console): %s\n", SDL_GetError());
    }
#endif

    SOLARIS_CONTROL_WINDOW control_window;
    control_window.start(direct_config.initial_cmt, direct_config.initial_qd);

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

    bool exit_requested = false;
    while(!host.quit_requested() && !exit_requested) {
        bool reset = false;
        bool reset_requested = false;

        host.poll(emu.key_buffer(), &reset);

        if(reset) {
            reset_requested = true;
        }
        if(control_window.pop_reset_request()) {
            reset_requested = true;
        }

        emu.lock_vm();
#ifdef DEBUG
        process_console_commands(console_state, emu, host, direct_config, audio_enabled, &exit_requested, &reset_requested);
#endif
        process_control_commands(&control_window, emu, host, direct_config, audio_enabled, &exit_requested, &reset_requested);

        if(reset_requested) {
            if(!reset_vm(emu, vm, audio_enabled, host.audio_rate(), sound_samples)) {
                fprintf(stderr, "VM reset failed\n");
                exit_requested = true;
            }
            frame_count = 0;
        }

        vm->run();

        if(audio_enabled && host.queued_audio_bytes() < audio_chunk_bytes * (uint32_t)audio_target_chunks) {
            queue_sound_chunks(emu, host, sound_samples, audio_chunk_bytes,
                               audio_target_chunks, audio_max_refill_chunks);
        }

        // frame skipping: only present every N frames to limit CPU usage. The VM runs at full speed regardless.
        if((frame_count++ & (direct_config.frame_skip - 1)) == 0) {
            vm->draw_screen();
            host.present(emu.screen_buffer(), screen_w, screen_h, screen_pitch);
        }

        emu.unlock_vm();

        host.delay_ms(frame_ms);
    }

    control_window.stop();
    delete vm;
    emu.set_vm(NULL);
    return 0;
}
