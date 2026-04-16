#define __DARWIN_NON_CANCELABLE 1
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef O_NONBLOCK
# define NBERRSTR "ioctl"
# include <sys/ioctl.h>
#else
# define NBERRSTR "fcntl"
#endif /* O_NONBLOCK */

#include "chatterbox.h"

static int newconnect(const char *host, const char *serv);
static void *readthread(void *sock);

static inline int setnbio(int fd) {
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

__attribute__((visibility("hidden")))
int clientloop(const char *host, const char *serv) {
	int sock = newconnect(host, serv);

	char *line = NULL;
	pthread_t thread;
	pthread_create(&thread, NULL, readthread, &line);

	struct pollfd pfd = {.fd = sock, .events = POLLHUP | POLLIN | POLLOUT,
		.revents = 0};

	char buf[1024];
	ssize_t readcount = 0;
	do {
		int pcount = poll(&pfd, 1, -1);

		if (pcount == 0)
			continue;
		else if (pcount < 0 && errno != EINTR)
			err(2, "poll");
		else if (pcount < 0)
			continue;

		if (pfd.revents & POLLHUP) {
			active = 0;
			break;
		}

		if (pfd.revents & POLLIN) {
			readcount = read(sock, buf, 1024);

			if (readcount > 0) {
				fwrite(buf, 1, readcount, stdout);
			} else if (readcount < 0 && errno != EAGAIN)
				err(2, "read");
			else if (readcount < 0)
				continue;
		}

		if (pfd.revents & POLLOUT) {
			char *line_out = __atomic_exchange_n(&line, NULL,
					__ATOMIC_RELAXED);
			if (line_out == NULL)
				continue;

			size_t len = strnlen(line_out, 1024);
			if (write(sock, line_out, len) < 0)
				err(2, "write");

			free(line_out);
		}
	} while (active);
	putchar('\n');

	pthread_kill(thread, SIGINT);
	pthread_join(thread, NULL);

	free(line);

	if (close(sock) < 0)
		err(2, "close");

	return 0;
}

static int newconnect(const char *host, const char *serv) {
	struct addrinfo *info;
	struct addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_V4MAPPED_CFG | AI_ALL;
	hints.ai_protocol = IPPROTO_TCP;

	int gairet = getaddrinfo(host, serv, &hints, &info);
	if (gairet)
		errx(2, "getaddrinfo: %s", gai_strerror(gairet));
	int sock = - 1;
	int conn = -1;
	for (struct addrinfo *i = info; i != NULL; i = i->ai_next) {
		sock = socket(i->ai_family, SOCK_STREAM, IPPROTO_TCP);
		if (sock < 0)
			continue;

		conn = connect(sock, i->ai_addr, i->ai_addrlen);
		if (conn) {
			close(sock);
			continue;
		} else
			break;
	}

	freeaddrinfo(info);

	if (sock < 0)
		err(2, "socket");

	if (conn)
		err(2, "connect");

	if (setnbio(sock) == -1)
		err(2, NBERRSTR);

	const int one = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)))
		err(2, "setsockopt");

	return sock;
}

static void *readthread(void *l) {
	char **line_out = l;

	size_t linecap = 0;
	char *line;
	while (active) {
		ssize_t linelen = getline(&line, &linecap, stdin);
		if (linelen < 0) {
			if (errno == EINTR)
				break;
			else
				err(2, "getline");
		}

		line[linelen - 1] = '\0';

		char *line_new = malloc(linelen);
		if (line_new == NULL)
				err(2, "malloc");

		memcpy(line_new, line, linelen);

		free(__atomic_exchange_n(line_out, line_new, __ATOMIC_RELAXED));
	}

	return NULL;
}
