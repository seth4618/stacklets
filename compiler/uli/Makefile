CC=gcc
# -DDEBUG -DTRACKER -DBENCHMARK -DCLEAN
CFLAGS=-g -O1 -DBENCHMARK -DTRACKER -I. -DULI
LIBS = -lpthread -l../../synch/libuli.a
OBJ = myfib.o seedStack.o readyQ.o stacklet.o debug.o tracker.o myassert.o spinlock.o msgs.o

fib: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) 

# pull in dependency info for *existing* .o files
-include $(addprefix .,$(OBJ:%.o=%.d))

%.o: %.c
	$(CC) -c -o $*.o $*.c $(CFLAGS)
	gcc -MM $(CFLAGS) $*.c > .$*.d

clean:
	rm -f *.o fib .*.d
