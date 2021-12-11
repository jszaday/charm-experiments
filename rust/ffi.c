#include <converse.h>

// this file provides functional bindings for
// converse's function-like macros

void CmiSetHandler_(void* msg, int handler) {
    CmiSetHandler(msg, handler);
}

void CmiSyncBroadcastAllAndFree_(int size, void* msg) {
    CmiSyncBroadcastAllAndFree(size, msg);
}

void CsdExitScheduler_(void) {
    CsdExitScheduler();
}
