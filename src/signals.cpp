#include "signals.h"

volatile sig_atomic_t g_sigint_flag = 0;

static void sigint_handler(int) {
	g_sigint_flag = 1;
	signal(SIGINT, SIG_DFL);
}

void register_signals() {
	signal(SIGINT, sigint_handler);
}
