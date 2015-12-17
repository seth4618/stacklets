CC=gcc
# -DDEBUG -DTRACKER -DBENCHMARK -DCLEAN -DULI -DSEED_STACK
CFLAGS=-g -O1 -DBENCHMARK -DTRACKER -I. -DULI -L../../sync -DDEBUG -DULI -DSEED_STACK
LIBS = -lpthread -luli
OBJ = myfib.o seedStack.o readyQ.o stacklet.o debug.o tracker.o myassert.o spinlock.o msgs.o

fib: $(OBJ) ../../sync/libuli.a
	$(CC) -o $@ $(OBJ) $(CFLAGS) $(LIBS) 

# pull in dependency info for *existing* .o files
-include $(addprefix .,$(OBJ:%.o=%.d))

%.o: %.c
	$(CC) -c -o $*.o $*.c $(CFLAGS)
	gcc -MM $(CFLAGS) $*.c > .$*.d

clean:
	rm -f *.o fib .*.d
