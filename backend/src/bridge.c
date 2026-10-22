#include "bridge.h"

#include <assert.h>
#include <stdbool.h>

static int vehicle_weight(VehicleType type) {
    return type == VEHICLE_TRUCK ? 15 : 5;
}

static bool may_enter(const Bridge *bridge, VehicleType type) {
    if (type == VEHICLE_TRUCK) return bridge->weight == 0;
    return bridge->weight + vehicle_weight(type) <= BRIDGE_CAPACITY && bridge->waiting_trucks == 0;
}

void bridge_init(Bridge *bridge) {
    pthread_mutex_init(&bridge->mutex, NULL);
    pthread_cond_init(&bridge->cars, NULL);
    pthread_cond_init(&bridge->trucks, NULL);
    bridge_reset(bridge);
}

void bridge_destroy(Bridge *bridge) {
    pthread_cond_destroy(&bridge->cars);
    pthread_cond_destroy(&bridge->trucks);
    pthread_mutex_destroy(&bridge->mutex);
}

void bridge_reset(Bridge *bridge) {
    pthread_mutex_lock(&bridge->mutex);
    bridge->weight = 0;
    bridge->waiting_cars = 0;
    bridge->waiting_trucks = 0;
    bridge->crossing_cars = 0;
    bridge->crossing_trucks = 0;
    pthread_mutex_unlock(&bridge->mutex);
}

void bridge_wake_all(Bridge *bridge) {
    pthread_mutex_lock(&bridge->mutex);
    pthread_cond_broadcast(&bridge->cars);
    pthread_cond_broadcast(&bridge->trucks);
    pthread_mutex_unlock(&bridge->mutex);
}

int bridge_acquire_interruptible(Bridge *bridge, VehicleType type, const atomic_bool *stop_requested) {
    pthread_cond_t *condition = type == VEHICLE_TRUCK ? &bridge->trucks : &bridge->cars;
    pthread_mutex_lock(&bridge->mutex);

    if (type == VEHICLE_TRUCK) bridge->waiting_trucks++;
    else bridge->waiting_cars++;

    /* Le mutex protège uniquement la réservation. Il est libéré pendant la
       traversée : le moniteur ne bloque donc pas toute la simulation. */
    while (!may_enter(bridge, type) && !(stop_requested && atomic_load(stop_requested))) {
        pthread_cond_wait(condition, &bridge->mutex);
    }

    if (type == VEHICLE_TRUCK) bridge->waiting_trucks--;
    else bridge->waiting_cars--;

    if (stop_requested && atomic_load(stop_requested)) {
        pthread_mutex_unlock(&bridge->mutex);
        return 0;
    }

    bridge->weight += vehicle_weight(type);
    if (type == VEHICLE_TRUCK) bridge->crossing_trucks++;
    else bridge->crossing_cars++;

    assert(bridge->weight >= 0);
    assert(bridge->weight <= BRIDGE_CAPACITY);
    assert(bridge->crossing_trucks <= 1);
    assert(!(bridge->crossing_trucks && bridge->crossing_cars));
    pthread_mutex_unlock(&bridge->mutex);
    return 1;
}

void bridge_acquire(Bridge *bridge, VehicleType type) {
    (void)bridge_acquire_interruptible(bridge, type, NULL);
}

void bridge_release(Bridge *bridge, VehicleType type) {
    pthread_mutex_lock(&bridge->mutex);
    bridge->weight -= vehicle_weight(type);
    if (type == VEHICLE_TRUCK) bridge->crossing_trucks--;
    else bridge->crossing_cars--;

    assert(bridge->weight >= 0);
    assert(bridge->weight <= BRIDGE_CAPACITY);

    if (bridge->waiting_trucks > 0 && bridge->weight == 0) pthread_cond_signal(&bridge->trucks);
    else if (bridge->waiting_trucks == 0) pthread_cond_broadcast(&bridge->cars);
    pthread_mutex_unlock(&bridge->mutex);
}

BridgeSnapshot bridge_snapshot(Bridge *bridge) {
    BridgeSnapshot snapshot;
    pthread_mutex_lock(&bridge->mutex);
    snapshot.weight = bridge->weight;
    snapshot.waiting_cars = bridge->waiting_cars;
    snapshot.waiting_trucks = bridge->waiting_trucks;
    snapshot.crossing_cars = bridge->crossing_cars;
    snapshot.crossing_trucks = bridge->crossing_trucks;
    pthread_mutex_unlock(&bridge->mutex);
    return snapshot;
}

int bridge_weight(Bridge *bridge) {
    return bridge_snapshot(bridge).weight;
}
