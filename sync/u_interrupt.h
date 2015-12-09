#ifndef U_INTERRUPT
#define U_INTERRUPT
#include "system.h"

typedef void (*callback_t)(void*);
typedef struct message_t
{
	callback_t callback;
	void *p;
} message;

static queue* msg_bufs[NUM_CORES];
static int flags[NUM_CORES];

void init_uint();

#ifdef SIM

/*
 * The SIM flag denotes that myapp is being run on simulated hardware (which in
 * this case is via gem5.
 */
#define DUI(x)  stacklet_uli_toggle(0, x)
#define EUI(x)  stacklet_uli_toggle(1, x)
#define SENDI(x, y) stacklet_sendi(x, y)
#define POLL()  printf("polling from within gem5...\n");
#define RETULI()  stacklet_retuli()
#define SETUPULI(x) stacklet_setupuli(x)
#define GETMYID stacklet_getcpuid()

#else

/*
 * The macros below are when running myapp as a regular C program.
 */
#define DUI(x)	dui(x)
#define EUI(x) 	eui(x)
#define SENDI(x, y) sendI(x, y)
#define POLL() poll()
#define RETULI()  return
#define SETUPULI(x) return
#define GETMYID get_myid()
#endif

void dui(int flag);
void eui(int flag);

void sendI(message *msg, int target);

void i_handler(int core_idx);
void poll();

#endif
