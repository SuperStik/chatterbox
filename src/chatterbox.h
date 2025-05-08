#ifndef CHATTERBOX_H
#define CHATTERBOX_H 1

#define CHAT_MAXCON 8

struct client {
	size_t msgleft;
	int fd;
};

#endif /* CHATTERBOX_H */
