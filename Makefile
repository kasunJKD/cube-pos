CC = gcc
CFLAGS = -g -Wall -Wextra -O2
LDFLAGS = -lSDL2 -lSDL2_ttf -lsqlite3

SRC = src/main.c
OUT = build/pos

all:
	mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

run: all
	./$(OUT)

clean:
	rm -rf build
