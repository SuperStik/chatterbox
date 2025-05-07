CC ?= cc
EXE := chatterbox
SRC := $(wildcard src/*.c)

.PHONY: clean

OUT_DIR := build
OUT := ${OUT_DIR}/${EXE}

${OUT}: ${SRC} ${OUT_DIR}
	${CC} ${SRC} -O2 -o $@

${OUT_DIR}:
	mkdir $@

clean:
	rm -fr ${OUT_DIR}
