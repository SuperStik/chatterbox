#define __DARWIN_NON_CANCELABLE 1
#include <err.h>
#include <signal.h>
#include <stdio.h>

static void setupsignals(void);

static char active = 1;

int main(int argc, char **argv) {
	setupsignals();
	return 0;
}

static void setinactive(int _) {
	(void)_;
	active = 0;
}

static void setupsignals(void) {
	sigset_t mask;
	sigemptyset(&mask);

	const struct sigaction action = {
		.sa_handler = setinactive,
		.sa_mask = mask,
		.sa_flags = SA_RESTART
	};

	if (sigaction(SIGINT, &action, NULL))
		err(2, "sigaction [L:%i F:%s]", __LINE__, __FILE__);

	if (sigaction(SIGQUIT, &action, NULL))
		err(2, "sigaction [L:%i F:%s]", __LINE__, __FILE__);
}
