#ifndef _USERINT_H_
#define _USERINT_H_

#ifdef SIM

#define DUI(x)	asm volatile("%0x822222 " ## x);
EUI
SENDI
#define POLL()

#endif

#define DUI(x)	dui(x)
EUI
SENDI
#define POLL() poll()

#endif

#endif
