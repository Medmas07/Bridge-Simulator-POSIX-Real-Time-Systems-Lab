#include "bridge.h"
#include "events.h"
#include "simulation.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Bridge *bridge;
    VehicleType type;
    pthread_mutex_t *gate_mutex;
    pthread_cond_t *gate_condition;
    int *entered;
    int *release;
    atomic_int *order;
    int *my_order;
} ThreadContext;

static void *held_vehicle(void *argument) {
    ThreadContext *context = argument;
    bridge_acquire(context->bridge, context->type);
    if (context->order) *context->my_order = atomic_fetch_add(context->order, 1) + 1;
    pthread_mutex_lock(context->gate_mutex);
    (*context->entered)++;
    pthread_cond_broadcast(context->gate_condition);
    while (!*context->release) pthread_cond_wait(context->gate_condition, context->gate_mutex);
    pthread_mutex_unlock(context->gate_mutex);
    bridge_release(context->bridge, context->type);
    return NULL;
}

static void wait_for_value(pthread_mutex_t *mutex, pthread_cond_t *condition, int *value, int expected) {
    pthread_mutex_lock(mutex);
    while (*value < expected) pthread_cond_wait(condition, mutex);
    pthread_mutex_unlock(mutex);
}

static void test_three_cars_and_zero_after_release(void) {
    Bridge bridge;
    pthread_t threads[3];
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    int entered = 0, release = 0;
    ThreadContext contexts[3];
    bridge_init(&bridge);
    for (int i = 0; i < 3; i++) {
        contexts[i] = (ThreadContext){ &bridge, VEHICLE_CAR, &mutex, &condition, &entered, &release, NULL, NULL };
        assert(pthread_create(&threads[i], NULL, held_vehicle, &contexts[i]) == 0);
    }
    wait_for_value(&mutex, &condition, &entered, 3);
    assert(bridge_weight(&bridge) == 15);
    pthread_mutex_lock(&mutex);
    release = 1;
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex);
    for (int i = 0; i < 3; i++) pthread_join(threads[i], NULL);
    assert(bridge_weight(&bridge) == 0);
    bridge_destroy(&bridge);
    pthread_cond_destroy(&condition);
    pthread_mutex_destroy(&mutex);
}

static void test_truck_priority(void) {
    Bridge bridge;
    pthread_t truck, car;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    int entered = 0, release = 0, truck_order = 0, car_order = 0;
    atomic_int order;
    atomic_init(&order, 0);
    bridge_init(&bridge);
    bridge_acquire(&bridge, VEHICLE_CAR);
    ThreadContext truck_context = { &bridge, VEHICLE_TRUCK, &mutex, &condition, &entered, &release, &order, &truck_order };
    ThreadContext car_context = { &bridge, VEHICLE_CAR, &mutex, &condition, &entered, &release, &order, &car_order };
    assert(pthread_create(&truck, NULL, held_vehicle, &truck_context) == 0);
    while (bridge_snapshot(&bridge).waiting_trucks != 1) sched_yield();
    assert(pthread_create(&car, NULL, held_vehicle, &car_context) == 0);
    while (bridge_snapshot(&bridge).waiting_cars != 1) sched_yield();
    bridge_release(&bridge, VEHICLE_CAR);
    wait_for_value(&mutex, &condition, &entered, 1);
    assert(truck_order == 1);
    assert(car_order == 0);
    pthread_mutex_lock(&mutex);
    release = 1;
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex);
    pthread_join(truck, NULL);
    pthread_join(car, NULL);
    assert(car_order == 2);
    assert(bridge_weight(&bridge) == 0);
    bridge_destroy(&bridge);
    pthread_cond_destroy(&condition);
    pthread_mutex_destroy(&mutex);
}

static void test_reset_joins_threads_and_changes_generation(void) {
    Simulation simulation;
    char event[1024];
    events_init();
    simulation_init(&simulation);
    unsigned previous_generation = simulation_generation(&simulation);
    assert(simulation_create_vehicle(&simulation, VEHICLE_CAR) > 0);
    simulation_reset(&simulation);
    assert(simulation_generation(&simulation) == previous_generation + 1);
    assert(bridge_weight(&simulation.bridge) == 0);
    assert(simulation.active_threads == 0);
    while (events_pop(event, sizeof(event))) {
        char expected[64];
        snprintf(expected, sizeof(expected), "\"generation\":%u", simulation_generation(&simulation));
        assert(strstr(event, expected) != NULL);
    }
    simulation_destroy(&simulation);
    events_destroy();
}

int main(void) {
    test_three_cars_and_zero_after_release();
    test_truck_priority();
    test_reset_joins_threads_and_changes_generation();
    puts("bridge monitor and lifecycle: ok");
    return 0;
}
