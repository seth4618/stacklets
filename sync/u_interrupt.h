#ifndef U_INTERRUPT
#define U_INTERRUPT

typedef void (*callback_t)(void*);
typedef struct message_t
{
	callback_t callback;
	void *p;
}message;

#ifdef SIM_ULI

/*
 * The SIM_ULI flag denotes that myapp is being run on simulated hardware (which in
 * this case is via gem5.
 */
#define DUI(x)  stacklet_uli_toggle(0, x)
#define EUI(x)  stacklet_uli_toggle(1, x)
#define SENDI(x, y) stacklet_sendi(x, y)
#define POLL()  printf("polling from within gem5...\n");
#define RETULI()  stacklet_retuli()
#define SETUPULI(x) stacklet_setupuli(x)
#define GETMYID() stacklet_getcpuid()
#define INIT_ULI(x)
#define GET_NR_CPUS()   stacklet_nrcpus()
#else

/*
 * The macros below are when running myapp as a regular C program.
 */
#define DUI(x)	dui(x)
#define EUI(x) 	eui(x)
#define SENDI(x, y) sendI(x, y)
#define POLL() poll()
#define RETULI()	return
#define SETUPULI(x)
#define GETMYID() get_myid()
#define INIT_ULI(x) init_uli(x)
#define GET_NR_CPUS()   get_nr_cpus()

void dui(int flag);
void eui(int flag);
void sendI(message *msg, int target);
void poll();
int get_myid(void);
void init_uli(int ncpus);
int get_nr_cpus(void);
#endif

#endif
