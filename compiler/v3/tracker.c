#ifdef TRACKER
#include "tracker.h"
#include "stacklet.h"
#include <stdlib.h>

TrackingInfo** trackingInfo;

void
trackerInit(int numThreads)
{
    trackingInfo = calloc(numThreads, sizeof(TrackingInfo*));
    int i;
    for (i=0; i<numThreads; i++) {
        trackingInfo[i] = calloc(1, sizeof(TrackingInfo));
    }
}

TrackingInfo*
getFinalTrackingInfo()
{
    TrackingInfo* finalTrackingInfo = calloc(1, sizeof(finalTrackingInfo));
    int i;
    for (i=0; i<numberOfThreads; i++) {
        finalTrackingInfo->fib += trackingInfo[i]->fib;
        finalTrackingInfo->fork += trackingInfo[i]->fork;
        finalTrackingInfo->firstReturn += trackingInfo[i]->firstReturn;
        finalTrackingInfo->secondReturn += trackingInfo[i]->secondReturn;
        finalTrackingInfo->suspend += trackingInfo[i]->suspend;
    }
    return finalTrackingInfo;
}
#endif
