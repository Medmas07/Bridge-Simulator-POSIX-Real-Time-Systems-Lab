#include "events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
static EventNode *head, *tail; static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER; static unsigned long sequence;
void events_init(void) { pthread_mutex_lock(&mutex); head=tail=0; sequence=0; pthread_mutex_unlock(&mutex); }
void events_push(const char *type, int id, VehicleType vehicle, int weight, const char *message) { EventNode *n=calloc(1,sizeof(*n)); const char *kind=vehicle==VEHICLE_TRUCK?"TRUCK":"CAR"; pthread_mutex_lock(&mutex); snprintf(n->json,sizeof(n->json),"{\"type\":\"%s\",\"sequence\":%lu,\"id\":%d,\"vehicleType\":\"%s\",\"weight\":%d,\"message\":\"%s\"}",type,++sequence,id,kind,weight,message?message:""); if(tail) tail->next=n; else head=n; tail=n; pthread_mutex_unlock(&mutex); }
int events_pop(char *out,int size) { pthread_mutex_lock(&mutex); if(!head){pthread_mutex_unlock(&mutex);return 0;} EventNode *n=head; head=n->next; if(!head)tail=0; pthread_mutex_unlock(&mutex); strncpy(out,n->json,(size_t)size-1); out[size-1]=0; free(n); return 1; }
void events_clear(void) { char x[512]; while(events_pop(x,sizeof(x))); }
