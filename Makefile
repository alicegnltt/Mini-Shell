CC=gcc
CFLAGS=-c -Wall

all: minishell
	$(CC) minishell.o -o a.out
minishell: minishell.c
	$(CC) $(CFLAGS) $^ -o minishell.o

clean:
	rm -f *.o
