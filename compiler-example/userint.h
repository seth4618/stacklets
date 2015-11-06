#ifndef _USERINT_H_
#define _USERINT_H_

#ifdef SIM

#define DUI(x)	asm volatile("%0x06 %[arg]",  : [arg] "=r" (x))
#define EUI(x)	asm volatile("%0x07 %[arg]",  : [arg] "=r" (x))
#define SENDI(x, y)	asm volatile("%0x16 %[arg] %[arg2]",  : [arg] "=r" (x) : [arg2] "=r" (y))

#define POLL()

#endif

#define DUI(x)	dui(x)
#define EUI(x) 	eui(x)
#define SENDI(x, y) sendi(x, y)
#define POLL() poll()

#endif