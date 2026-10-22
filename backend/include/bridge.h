#ifndef BRIDGE_H
#define BRIDGE_H
#include <pthread.h>
#define BRIDGE_CAPACITY 15
typedef enum { VEHICLE_CAR, VEHICLE_TRUCK } VehicleType;
typedef struct { pthread_mutex_t mutex; pthread_cond_t cars; pthread_cond_t trucks; int weight; int waiting_cars; int waiting_trucks; int crossing_cars; } Bridge;
void bridge_init(Bridge *b); void bridge_destroy(Bridge *b);
void bridge_acquire(Bridge *b, VehicleType type); void bridge_release(Bridge *b, VehicleType type);
int bridge_weight(Bridge *b);
#endif
