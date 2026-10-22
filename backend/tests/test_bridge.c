#include "bridge.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
static void *car(void *p){bridge_acquire(p,VEHICLE_CAR); bridge_release(p,VEHICLE_CAR); return 0;}
int main(void){Bridge b; bridge_init(&b); pthread_t t[3]; for(int i=0;i<3;i++)assert(!pthread_create(&t[i],0,car,&b)); for(int i=0;i<3;i++)pthread_join(t[i],0); assert(bridge_weight(&b)==0); bridge_destroy(&b); puts("monitor: ok"); return 0;}
