#define __DARWIN_NON_CANCELABLE 1
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef O_NONBLOCK
# define NBERRSTR "ioctl"
# include <sys/ioctl.h>
#else
# define NBERRSTR "fcntl"
#endif /* O_NONBLOCK */

#include "chatterbox.h"

static void bindsocket(int sock, const char *host, const char *serv);

static void acceptclient(int kq, int listener);
static void writeclient(int client, const char *msg, ssize_t msgsize,
		struct client *, int clientno);

static size_t readclient(int client, char *msg, struct client *);

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

static struct client clients[CHAT_MAXCON];

__attribute__((visibility("hidden")))
int serverloop(const char *host, const char *serv) {
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
	int clientno = -1;
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
					struct client *cl = eventlist[i].udata;
					clientno = cl - clients;

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
							eventlist[i].udata,
							clientno);
					size_t i;
					for (i = 0; i < CHAT_MAXCON; ++i) {
						if (clients[i].fd < 0)
							continue;

						if (clients[i].msgleft != 0)
							break;
					}

					if (i >= CHAT_MAXCON)
						readstate = CHATSTATE_RDONLY;

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
		struct client *client, int clientno) {
	if (msgsize <= 0)
		return;

	dprintf(fd, "Client %i: ", clientno);
	ssize_t writelen = write(fd, &msg[msgsize - client->msgleft], msgsize);
	if (writelen < 0) {
		warn("write");
		return;
	}

	client->msgleft = client->msgleft - writelen;	
}

static size_t readclient(int fd, char *msg, struct client *client) {
	ssize_t msgsize = read(fd, msg, 1024);
	if (msgsize < 0) {
		warn("read");
		goto disconnect;
	} else if (msgsize == 0) {
disconnect:
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
