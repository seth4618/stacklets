# userthreads

a user level threads package

mythreads.h- header file for package
mythreads_64.c- source code of package (64 bit version)
sbuffree.c- source code of buffer (modified producer-consumer model)
sbuffree.h- header file for buffer code

Application Programs:

1) tt_fibonacci_64.c:-
   to make: make -f Makefile.fibonacci_m64
   executable: tt_fibonacci_64
   to run: ./tt_fibonacci_64 <number whose fib is to be calculated> <number of pthreads>

2) tt_create_join.c:-
   to make: make -f Makefile.createandjoin
   executable: tt_create_join
   to run: ./tt_create_join <number of mythreads> <number of pthreads>

Plots:
my_script: to plot time of execution v/s number of pthreads
my_script_ratio: ratio of time of execution of n pthreads to one pthread v/s number of pthreads