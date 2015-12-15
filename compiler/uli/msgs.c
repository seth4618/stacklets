#include "msgs.h"
#include <stdlib.h>
#include "myassert.h"

extern __thread int threadId;
static __thread int uiFlagCtr;
__thread void** sysStackPtr;

typedef struct {
    int next;
    int* buffer;
} Tracker;

#define TRACK_SIZE 100
static Tracker* uiTracker;

void
showTrack(int x)
{
    int i;
    for (i=uiTracker[x].next; i>=0; i--) {
	int y = uiTracker[x].buffer[i-1];
	char* type = (y&0x1000) ? "EUI" : "DUI";
	dprintLine("%s(%d)\n", type, y&0xfff);
    }
    if (uiTracker[x].next > 15) return;
    dprintLine("----------wrap\n");
    for (i=0; i<20; i++) {
	int y = uiTracker[x].buffer[TRACK_SIZE-(1+i)];
	char* type = (y&0x1000) ? "EUI" : "DUI";
	dprintLine("%s(%d)\n", type, y&0xfff);
    }
}

void
insertTrack(int x)
{
    uiTracker[threadId].buffer[uiTracker[threadId].next] = x;
    uiTracker[threadId].next++;
    if (uiTracker[threadId].next == TRACK_SIZE) uiTracker[threadId].next = 0;
}

void
myeui(int x)
{
    insertTrack(x|0x1000);
    uiFlagCtr--;
    if (uiFlagCtr < 0) {
	dprintLine("EUI FAILS\n");
	showTrack(threadId);
    }
    myassert(uiFlagCtr >= 0, "myeui goes below zero");
    if (uiFlagCtr == 0) EUI(0);
}

void
mydui(int x)
{
    insertTrack(x);
    if (uiFlagCtr == 0) DUI(0);
    uiFlagCtr++;
}

void checkUIMask(int ln)
{
    if (uiFlagCtr != 0) {
	dprintLine("check of UI  mask FAILS:%d @%d\n", uiFlagCtr, ln);
	showTrack(threadId);
	myassert(0, "");
    }
}

static BasicMessage** msgPool;

void
initMsgs(int nt)
{
    if (nt > 0) {
	msgPool = calloc(nt, sizeof(BasicMessage*));
	uiTracker = calloc(nt, sizeof(Tracker));
	int i;
	for (i=0; i<nt; i++) {
	    uiTracker[i].buffer = calloc(TRACK_SIZE, sizeof(int));
	    uiTracker[i].next = 0;
	}
    } else {
	dprintLine("Initing msgs\n");
	sysStackPtr = calloc(20, sizeof(void*));
	uiFlagCtr = 0;
    }
}

#define ALLOC_COUNT 10

// allocate a msg from poll big enough to handle any of the message defined here.
// fill in info from base (which sets ptr in base to returned ptr
void* 
getMsg(callback_t handler)
{
    if (msgPool[threadId] == NULL) {
	BasicMessage* chunk = (void*)calloc(ALLOC_COUNT, MAX_MSG_SIZE);
	int i;
	for (i=0; i<ALLOC_COUNT-1; i++) {
	    ((BasicMessage*)(((char*)chunk)+MAX_MSG_SIZE*i))->next = ((BasicMessage*)(((char*)chunk)+MAX_MSG_SIZE*(i+1)));
	}
	((BasicMessage*)(((char*)chunk)+MAX_MSG_SIZE*(ALLOC_COUNT-1)))->next = NULL;
	mydui(0);
	myassert(msgPool[threadId] == NULL, "race condition allocating more msg buffers?");
	msgPool[threadId] = chunk;
	myeui(0);
    }
    mydui(1);
    BasicMessage* chunk = msgPool[threadId];
    myassert(chunk != NULL, "race condition getting msg buffer?");
    msgPool[threadId] = chunk->next;
    myeui(1);
    chunk->systemMsg.callback = handler;
    chunk->systemMsg.p = chunk;
    return chunk;
}

void 
freeMsgBuffer(BasicMessage* msg)
{
    mydui(2);
    msg->next = msgPool[threadId];
    msgPool[threadId] = msg;
    myeui(2);
}

// get a reply msg for this particular interrupt

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
