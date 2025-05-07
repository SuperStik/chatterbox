CC ?= cc
EXE := chatterbox
SRC := $(wildcard src/*.c)

.PHONY: clean

OUT_DIR := build
OUT := ${OUT_DIR}/${EXE}

O ?= 2

${OUT}: ${SRC} ${OUT_DIR}
	${CC} ${SRC} -O$O ${CCFLAGS} -o $@

${OUT_DIR}:
	mkdir $@

clean:
	rm -fr ${OUT_DIR}
