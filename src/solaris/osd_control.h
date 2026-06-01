/*
 * Control window for SHARP MZ-1500 on Solaris using GTK2+.
 * Copyright (c) 2026 Suikasama1968
 */

#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <pthread.h>

// GTK2+ control window for Solaris host.
class SOLARIS_CONTROL_WINDOW {
public:
    SOLARIS_CONTROL_WINDOW();
    ~SOLARIS_CONTROL_WINDOW();

    bool start(const std::string& initial_cmt = std::string(), const std::string& initial_qd = std::string());
    void stop();
    bool stop_requested() const;
    bool pop_reset_request();
    void request_reset();
    bool pop_command(std::string *command);
    void push_command(const std::string& command);

private:
    pthread_t thread_;
    bool thread_started_;
    std::atomic<bool> stop_requested_;
    std::atomic<bool> reset_requested_;
    std::mutex mutex_;
    std::queue<std::string> commands_;
    std::string initial_cmt_;
    std::string initial_qd_;

    void thread_main();
    static void* thread_proc(void *arg);
};
