SHELL = /bin/bash
CC = gcc
CFLAGS = -g
#CFLAGS = -g -pthread
SRC = $(wildcard *.c)
EXE = $(patsubst %.c, %, $(SRC))

all: ${EXE}

%:	%.c
	${CC} ${CFLAGS} $@.c -o $@ -I/usr/include/openssl -lssl -lcrypto -L/usr/lib64

clean:
	rm ${EXE}
