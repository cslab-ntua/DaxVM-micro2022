CC=gcc
CFLAGS=-g -Wall -pthread
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


#all: $(patsubst %.c,%,$(wildcard *.c))

#%: %.o
#	$(CC) -o $@ $<


#objects = read
#all: $(objects)

#$(objects): %: %.c
#	$(CC) $(CFLAGS) -o $@ $<

#clean:
#	rm -f $(objects)
