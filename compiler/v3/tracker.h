#ifdef TRACKER
#pragma once

typedef struct {
    int fib;
    int fork;
    int firstReturn;
    int secondReturn;
    int suspend;
} TrackingInfo;
extern TrackingInfo** trackingInfo;

void trackerInit(int numThreads);
TrackingInfo* getFinalTrackingInfo();

#endif
