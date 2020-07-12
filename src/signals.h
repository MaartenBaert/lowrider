#pragma once

#include <csignal>

extern volatile sig_atomic_t g_sigint_flag;

void register_signals();
