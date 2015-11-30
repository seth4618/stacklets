CC=gcc
CFLAGS=-g -O1
LIBS = -lpthread
DEPS = myfib.h seedStack.h readyQ.h stacklet.h debug.h
OBJ = myfib.o seedStack.o readyQ.o stacklet.o debug.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fib: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f *.o fib
