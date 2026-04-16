#define __DARWIN_NON_CANCELABLE 1
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "chatterbox.h"

static void setupsignals(void);
static void printhelp(const char *argv0);

static struct client clients[CHAT_MAXCON];

char active = 1;

int main(int argc, char **argv) {
	char server = 0;
	int opt;
	const char *host = NULL;
	const char *serv = "8196";
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

	if (server)
		return serverloop(host, serv);
	else
		return clientloop(host, serv);
}

static void printhelp(const char *argv0) {
	fprintf(stderr, "usage: %s [-h] [-c | -s] [-a host] [-p service]\n",
			argv0);
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
		.sa_flags = 0
	};

	if (sigaction(SIGINT, &action, NULL))
		err(2, "sigaction");

	if (sigaction(SIGQUIT, &action, NULL))
		err(2, "sigaction");
}
