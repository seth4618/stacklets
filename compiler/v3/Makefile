CC=gcc
# -DDEBUG -DTRACKER -DBENCHMARK
CFLAGS=-g -O1 -DTRACKER -DBENCHMARK
LIBS = -lpthread
OBJ = myfib.o seedStack.o readyQ.o stacklet.o debug.o

fib: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

# pull in dependency info for *existing* .o files
-include $(addprefix .,$(OBJ:%.o=%.d))

%.o: %.c
	$(CC) -c -o $*.o $*.c $(CFLAGS)
	gcc -MM $(CFLAGS) $*.c > .$*.d

clean:
	rm -f *.o fib .*.d
