#/* ########################################################################## */
#/* (C) UPMC, 2010-2011                                                        */
#/*     Authors:                                                               */
#/*       Jean-Pierre Lozi <jean-pierre.lozi@lip6.fr>                          */
#/*       Gaël Thomas <gael.thomas@lip6.fr>                                   */
#/*       Florian David <florian.david@lip6.fr>                                */
#/*       Julia Lawall <julia.lawall@lip6.fr>                                  */
#/*       Gilles Muller <gilles.muller@lip6.fr>                                */
#/* -------------------------------------------------------------------------- */
#/* ########################################################################## */

ARCH=$(arch)

ROOT=..

include $(ROOT)/Makefile.config

PROJECT=liblock

BIN=test-$(PROJECT)
BOOTSTRAP=$(BIN)
MAIN=main.o
OBJ=liblock.o flatcombining.o spinlock.o mcs.o posix.o mcstp.o ccsynch.o dsmsynch.o rcl.o our_rcl.o
OBJ32=liblock32.o flatcombining32.o spinlock32.o mcs32.o posix32.o mcstp32.o ccsynch32.o dsmsynch32.o rcl32.o
OBJ64=liblock64.o flatcombining64.o spinlock64.o mcs64.o posix64.o mcstp64.o ccsynch64.o dsmsynch64.o rcl64.o

DEPEND_OPTIONS=-MMD -MP -MF ".$*.d.tmp" -MT "$*.o" -MT ".$*.d"
DOM=then mv -f ".$*.d.tmp" ".$*.d"; else rm -f ".$*.d.tmp"; exit 1; fi

CFLAGS += -g -Wall -D_GNU_SOURCE -fPIC -O3 $(ADDITIONAL_CFLAGS)
LDFLAGS +=

ifeq ($(shell uname),SunOS)
CFLAGS += -DNO_FIFO
LDFLAGS += -lrt -lcpc
else
LDFLAGS += -lpapi -lpthread -lnuma -L.
endif

ifneq ($(shell arch),sun4)
CFLAGS += -m64
else
BOOTSTRAP += $(PROJECT)64.so
endif

Echo=@echo [$(PROJECT)]:

ifndef VERBOSE
  Verb := @
endif

DEPENDENCIES=$(patsubst %.o, .%.d, $(OBJ))

.PHONY: all bootstrap tidy clean distclean
.SECONDARY:
.SUFFIXES:

all: bootstrap

bootstrap: $(BOOTSTRAP)

#$(BIN): $(MAIN) $(PROJECT).a
#	$(Echo) Linking $@
#	$(Verb) g++ -o $@ $^ $(LDFLAGS)

$(BIN): $(MAIN) $(PROJECT).so
	$(Echo) Linking $@
	$(Verb) g++ -o $@ $^ $(LDFLAGS) $(PROJECT).so

$(PROJECT).a: $(PROJECT)-single-object-file.o
	$(Echo) Archiving $@
	$(Verb) ar rcsf $@ $^

$(PROJECT)-single-object-file.o: $(OBJ)
	$(Echo) Building complete $@
	$(Verb) g++ -r -nostdlib -nodefaultlibs -nostartfiles $(LDFLAGS) -o $@ $(OBJ)

$(PROJECT)32.so: $(OBJ32)
	$(Echo) Building complete $@
	$(Verb) g++ -m32 -shared $(LDFLAGS) -o $@ $(OBJ32)

$(PROJECT)64.so: $(OBJ64)
	$(Echo) Building complete $@
	$(Verb) g++ -m64 -shared $(LDFLAGS) -o $@ $(OBJ64)

$(PROJECT).so: $(OBJ)
	$(Echo) Building complete $@
	$(Verb) g++ -shared $(LDFLAGS) -o $@ $(OBJ) $(ROOT)/$(PROJECT)/uli.so

%32.o: %.c Makefile $(ROOT)/Makefile.config
	$(Echo) Compiling $<
	$(Verb) if gcc -m32 $(CFLAGS) $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%64.o: %.c Makefile $(ROOT)/Makefile.config
	$(Echo) Compiling $<
	$(Verb) if gcc -m64 $(CFLAGS) $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

%.o: %.c Makefile $(ROOT)/Makefile.config
	$(Echo) Compiling $<
	$(Verb) if gcc $(CFLAGS) $(DEPEND_OPTIONS) -c "$<" -o "$@"; $(DOM)

tidy:
	rm -f *~ \#*

clean:
	$(Echo) Cleaning compilation files
	$(Verb) rm -f *.o .*.d $(MAIN)

distclean: clean
	$(Echo) Cleaning distribution
	$(Verb) rm -f $(BIN) $(PROJECT).a $(PROJECT).so $(PROJECT)32.so $(PROJECT)64.so

ifneq ($(MAKECMDGOALS),tidy)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPENDENCIES)
endif
endif
endif

