#define __DARWIN_NON_CANCELABLE 1
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
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

static void setupsignals(void);
static void printhelp(const char *argv0);

static int clientloop(const char *host, const char *serv);
static int serverloop(const char *host, const char *serv);

static void bindsocket(int sock, const char *host, const char *serv);
static int newconnect(const char *host, const char *serv);

static void acceptclient(int kq, int listener);
static void writeclient(int client, const char *msg, ssize_t msgsize,
		struct client *);
static size_t readclient(int client, char *msg, struct client *);
static struct client clients[CHAT_MAXCON];

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

static int serverloop(const char *host, const char *serv) {
	for (size_t i = 0; i < CHAT_MAXCON; ++i)
		clients[i].fd = -1;

	int listener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (listener < 0)
		err(2, "socket");

	const int off;
	if (setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)))
		err(2, "setsockopt");

	bindsocket(listener, host, serv);

	if (setnbio(listener) == -1)
		err(2, NBERRSTR);

	if (listen(listener, SOMAXCONN))
		err(2, "listen");

	int kq = kqueue();
	if (kq < 0)
		err(2, "kqueue");

	struct kevent event = {0};
	EV_SET(&event, listener, EVFILT_READ, EV_ADD, 0, 0, NULL);

	if (kevent(kq, &event, 1, NULL, 0, NULL) < 0)
		err(2, "kevent");

	char chatmsg[256];
	ssize_t msgsize = 0;
	int readstate = CHATSTATE_RDONLY;
	while(active) {
		struct kevent eventlist[CHAT_MAXCON];
		memset(eventlist, 0, sizeof(eventlist));

		int nevents = kevent(kq, NULL, 0, eventlist, 4, NULL);
		if (nevents < 0 && errno == EINTR)
			continue;
		else if (nevents < 0)
			err(2, "kevent");

		for (int i = 0; i < nevents; ++i) {
			if (eventlist[i].ident == listener) {
				acceptclient(kq, eventlist[i].ident);
				continue;
			}
			switch(eventlist[i].filter) {
				case EVFILT_READ:
					if (readstate & CHATSTATE_WRONLY)
						continue;

					readstate = CHATSTATE_NONE;

					msgsize = readclient(eventlist[i].ident,
							chatmsg,
							eventlist[i].udata);

					for (size_t i = 0; i < CHAT_MAXCON;
							++i) {
						if (clients[i].fd < 0)
							continue;

						clients[i].msgleft = msgsize;
					}

					readstate = CHATSTATE_WRONLY;

					break;
				case EVFILT_WRITE:
					if (readstate & CHATSTATE_RDONLY)
						continue;

					writeclient(eventlist[i].ident, chatmsg,
							msgsize,
							eventlist[i].udata);
					size_t i;
					for (i = 0; i < CHAT_MAXCON; ++i) {
						if (clients[i].fd < 0)
							continue;

						if (clients[i].msgleft != 0)
							break;
					}

					if (i >= CHAT_MAXCON) {
						puts("GOING READONLY!");
						readstate = CHATSTATE_RDONLY;
					}

					break;
			}
		}
	}

	if (close(kq))
		err(2, "close");

	if (close(listener))
		err(2, "close");

	return 0;
}

static void acceptclient(int kq, int listener) {
	int client = accept(listener, NULL, NULL);
	if (client < 0) {
		warn("accept");
		return;
	}

	int id;
	for (id = 0; id < CHAT_MAXCON; ++id) {
		if (clients[id].fd < 0)
			break;
	}

	if (id >= CHAT_MAXCON) {
		warnx("Rejecting client on socket %i: Too many clients!",
				client);
		close(client);
		return;
	}

	clients[id].fd = client;
	clients[id].msgleft = 0;

	if (setnbio(client) == -1)
		err(2, NBERRSTR);

	struct kevent event[2] = {{0}, {0}};
	EV_SET(&event[0], client, EVFILT_WRITE, EV_ADD, 0, 0, &(clients[id]));
	EV_SET(&event[1], client, EVFILT_READ, EV_ADD, 0, 0, &(clients[id]));
	if (kevent(kq, event, 2, NULL, 0, NULL) < 0)
		err(2, "kevent");

	printf("Client %i connected\n", id);
}

static void writeclient(int fd, const char *msg, ssize_t msgsize,
		struct client *client) {
	ssize_t writelen = write(fd, &msg[msgsize - client->msgleft], msgsize);
	if (writelen < 0)
		err(2, "write");

	client->msgleft = client->msgleft - writelen;	
}

static size_t readclient(int fd, char *msg, struct client *client) {
	ssize_t msgsize = read(fd, msg, 1024);
	if (msgsize < 0)
		err(2, "read");
	else if (msgsize == 0) {
		printf("Client %ti disconnected\n", client - clients);

		client->fd = -1;

		if (close(fd))
			err(2, "close");
	} else {
		printf("Client %ti: ", client - clients);
		fwrite(msg, 1, msgsize, stdout);
		putchar('\n');
	}

	return msgsize;
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
