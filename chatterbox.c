#define __DARWIN_NON_CANCELABLE 1
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static void setupsignals(void);
static void printhelp(const char *argv0);

static char active = 1;

int main(int argc, char **argv) {
	char server = 0;
	int opt;
	const char *host = NULL;
	const char *serv = NULL;
	while((opt = getopt(argc, argv, "a:chp:s")) > -1 ) {
		switch(opt) {
			case 'a':
				host = optarg;
				break;
			case 'c':
				server = 0;
				break;
			case 'h':
				printhelp(argv[0]);
				return 0;
			case 'p':
				serv = optarg;
				break;
			case 's':
				server = 1;
				break;
			default:
				printhelp(argv[0]);
				return 1;
		}

	}
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

static void printhelp(const char *argv0) {
	fprintf(stderr, "usage: %s [-h] [-c | -s] [-a host] [-p service]\n",
			argv0);
}
