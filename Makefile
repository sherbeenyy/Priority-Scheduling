CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11
INCLUDES=-Iinclude
SRC=src/main.c src/priority_scheduler.c
OUT=bin/scheduler

all: build

build:
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) -o $(OUT)

clean:
	rm -rf bin
