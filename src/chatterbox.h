#ifndef CHATTERBOX_H
#define CHATTERBOX_H 1

#define CHAT_MAXCON 8

#define CHATSTATE_FULL 0
#define CHATSTATE_WRONLY 1
#define CHATSTATE_RDONLY 2
#define CHATSTATE_NONE CHATSTATE_WRONLY|CHATSTATE_RDONLY
struct client {
	size_t msgleft;
	int fd;
};

#endif /* CHATTERBOX_H */
