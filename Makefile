CC ?= cc
EXE := chatterbox
SRC := $(wildcard src/*.c)

.PHONY: clean

OUT_DIR := build
OUT := ${OUT_DIR}/${EXE}

O ?= 2

override CCFLAGS += -flto

${OUT}: ${SRC} ${OUT_DIR}
	${CC} ${SRC} -O$O -o $@ ${CCFLAGS}

${OUT_DIR}:
	mkdir $@

clean:
	rm -fr ${OUT_DIR}
