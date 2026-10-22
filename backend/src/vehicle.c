#include "vehicle.h"
#include "events.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
static pthread_mutex_t ids=PTHREAD_MUTEX_INITIALIZER; static int next_id=1;
static void *run(void *arg) { Vehicle *v=arg; int weight=v->type==VEHICLE_TRUCK?15:5; events_push("VEHICLE_WAITING",v->id,v->type,weight,"Demande d'accès au moniteur"); bridge_acquire(v->bridge,v->type); events_push("VEHICLE_ENTERED",v->id,v->type,weight,"Véhicule admis"); events_push("BRIDGE_WEIGHT_CHANGED",v->id,v->type,bridge_weight(v->bridge),"Charge mise à jour"); usleep(1800000); bridge_release(v->bridge,v->type); events_push("VEHICLE_EXITED",v->id,v->type,0,"Véhicule sorti"); free(v); return 0; }
void vehicle_start(Bridge *bridge, VehicleType type) { Vehicle *v=calloc(1,sizeof(*v)); pthread_mutex_lock(&ids); v->id=next_id++; pthread_mutex_unlock(&ids); v->bridge=bridge; v->type=type; events_push("VEHICLE_CREATED",v->id,type,type==VEHICLE_TRUCK?15:5,"Thread POSIX créé"); pthread_t thread; if(pthread_create(&thread,0,run,v)==0) pthread_detach(thread); else free(v); }
