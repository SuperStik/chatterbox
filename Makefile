CC ?= cc
EXE := chatterbox
SRC_DIR := src
SRC := $(wildcard ${SRC_DIR}/*.c)
OBJ_DIR = ${OUT_DIR}/objects
OBJ = $(patsubst src/%.c,${OBJ_DIR}/%.o,${SRC})

override LIB += pthread
LIB_FL := $(patsubst %,-l%,${LIB})

.PHONY: clean

OUT_DIR := build
OUT := ${OUT_DIR}/${EXE}

O ?= 2

override CCFLAGS += -flto

${OUT}: ${OBJ}
	${CC} $^ -O$O -o $@ ${LIB_FL} ${CCFLAGS}

${OBJ_DIR}/%.o: ${SRC_DIR}/%.c ${OBJ_DIR}
	${CC} $< -O$O -o $@ -c ${CCFLAGS}

${OBJ_DIR}:
	mkdir -p $@

${OUT_DIR}:
	mkdir $@

clean:
	rm -fr ${OUT_DIR}
