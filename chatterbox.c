#define __DARWIN_NON_CANCELABLE 1
#include <err.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef O_NONBLOCK
# define NBERRSTR "ioctl"
# include <sys/ioctl.h>
#else
# define NBERRSTR "fcntl"
#endif

static void setupsignals(void);
static void printhelp(const char *argv0);

static int clientloop(const char *host, const char *serv);
static int serverloop(const char *host, const char *serv);

static void bindsocket(int sock, const char *host, const char *serv);

static int setnbio(int fd) {
	int flags;
#ifdef O_NONBLOCK
	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		flags = 0;

	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIONBIO, &flags);
#endif
}

static char active = 1;

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

static int clientloop(const char *host, const char *serv) {
	return 0;
}

static int serverloop(const char *host, const char *serv) {
	int listener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (listener < 0)
		err(2, "socket");

	const int off;
	if (setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)))
		err(2, "setsockopt");

	bindsocket(listener, host, serv);

	if (setnbio(listener) == -1)
		err(2, NBERRSTR);

	if (close(listener))
		err(2, "close");

	return 0;
}

static void bindsocket(int sock, const char *host, const char *serv) {
	struct addrinfo *info;
	struct addrinfo hints = {0};
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_V4MAPPED_CFG;
	hints.ai_protocol = IPPROTO_TCP;

	int gairet = getaddrinfo(host, serv, &hints, &info);
	if (gairet)
		errx(2, "getaddrinfo: %s", gai_strerror(gairet));

	int bindret = -1;
	for (struct addrinfo *i = info; i != NULL; i = i->ai_next) {
		bindret = bind(sock, i->ai_addr, i->ai_addrlen);

		if (bindret == 0)
			break;
	}

	if (bindret)
		err(2, "bind");

	freeaddrinfo(info);
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
		err(2, "sigaction");

	if (sigaction(SIGQUIT, &action, NULL))
		err(2, "sigaction");
}

static void printhelp(const char *argv0) {
	fprintf(stderr, "usage: %s [-h] [-c | -s] [-a host] [-p service]\n",
			argv0);
}
