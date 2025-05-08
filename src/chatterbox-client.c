#define __DARWIN_NON_CANCELABLE 1
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
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

__attribute__((visibility("hidden")))
int clientloop(const char *host, const char *serv) {
	int sock = newconnect(host, serv);

	if (write(sock, "HELLO", strlen("HELLO")) < 0)
		err(2, "write");

	char buf[1024];
	ssize_t readcount = 0;
	do {
		readcount = read(sock, buf, 1024);

		if (readcount > 0)
			fwrite(buf, 1, readcount, stdout);
		else if (readcount < 0)
			err(2, "read");
	} while (readcount > 0);
	putchar('\n');

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

	return sock;
}
