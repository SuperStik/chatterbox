CC ?= cc
EXE := chatterbox
SRC_DIR := src
SRC := $(wildcard ${SRC_DIR}/*.c)

.PHONY: clean

OUT_DIR := build
OBJ_DIR := ${OUT_DIR}/objects
OBJ := $(patsubst src/%.c,${OBJ_DIR}/%.o,${SRC})
OUT := ${OUT_DIR}/${EXE}

O ?= 2

override CCFLAGS += -flto

${OUT}: ${OBJ}
	${CC} $^ -O$O -o $@ ${CCFLAGS}

${OBJ_DIR}/%.o: ${SRC_DIR}/%.c ${OBJ_DIR}
	${CC} $< -O$O -o $@ -c ${CCFLAGS}

${OBJ_DIR}: ${OUT_DIR}
	mkdir $@

${OUT_DIR}:
	mkdir $@

clean:
	rm -fr ${OUT_DIR}
