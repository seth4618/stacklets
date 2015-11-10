CC=gcc-4.8
CFLAGS=-g -DDEBUG -O
DEPS = myfib.h seedStack.h readyQ.h
OBJ = myfib.o seedStack.o readyQ.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fib: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

myfib:
	$(CC) $(CFLAGS) -S myfib.c

clean:
	rm -f *.o fib
