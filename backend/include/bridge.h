#ifndef BRIDGE_H
#define BRIDGE_H

#include <pthread.h>
#include <stdatomic.h>

#define BRIDGE_CAPACITY 15

typedef enum { VEHICLE_CAR, VEHICLE_TRUCK } VehicleType;

typedef struct {
    int weight;
    int waiting_cars;
    int waiting_trucks;
    int crossing_cars;
    int crossing_trucks;
} BridgeSnapshot;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cars;
    pthread_cond_t trucks;
    int weight;
    int waiting_cars;
    int waiting_trucks;
    int crossing_cars;
    int crossing_trucks;
} Bridge;

void bridge_init(Bridge *bridge);
void bridge_destroy(Bridge *bridge);
void bridge_reset(Bridge *bridge);
void bridge_wake_all(Bridge *bridge);

void bridge_acquire(Bridge *bridge, VehicleType type);
int bridge_acquire_interruptible(Bridge *bridge, VehicleType type, const atomic_bool *stop_requested);
void bridge_release(Bridge *bridge, VehicleType type);

int bridge_weight(Bridge *bridge);
BridgeSnapshot bridge_snapshot(Bridge *bridge);

#endif
