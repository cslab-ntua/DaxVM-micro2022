CC=gcc
CFLAGS=-g -Wall -pthread -march=native
OBJ=.
SRC=.
BINDIR=.
BIN=$(BINDIR)/read

SRCS=$(wildcard $(SRC)/*.c)
OBJS=$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRCS))

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o read
