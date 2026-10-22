#include "scenarios.h"

#include "events.h"
#include "scenario_common.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Simulation *simulation;
    ScenarioBarrier barrier;
    pthread_mutex_t weight_mutex;
    int weight;
    int vehicle_id;
} RaceContext;

static void *race_vehicle_thread(void *argument) {
    RaceContext *context = argument;
    int id;
    int observed;
    char fields[512];
    simulation_thread_started(context->simulation);
    pthread_mutex_lock(&context->weight_mutex);
    id = context->vehicle_id++;
    observed = context->weight;
    pthread_mutex_unlock(&context->weight_mutex);
    simulation_vehicle_add(context->simulation, id, VEHICLE_CAR, simulation_generation(context->simulation));
    snprintf(fields, sizeof(fields), "\"id\":%d,\"vehicleType\":\"CAR\",\"vehicleState\":\"WAITING\",\"message\":\"Voiture de demonstration creee\"", id);
    events_emit(simulation_generation(context->simulation), "VEHICLE_CREATED", fields);
    snprintf(fields, sizeof(fields), "\"id\":%d,\"observedWeight\":%d,\"message\":\"Lecture non atomique: 10 tonnes\"", id, observed);
    events_emit(simulation_generation(context->simulation), "RACE_READ", fields);
    scenario_barrier_wait(&context->barrier);
    snprintf(fields, sizeof(fields), "\"id\":%d,\"decision\":\"ADMIT\",\"message\":\"10 + 5 semble autorise\"", id);
    events_emit(simulation_generation(context->simulation), "RACE_DECISION", fields);
    scenario_barrier_wait(&context->barrier);
    pthread_mutex_lock(&context->weight_mutex);
    context->weight += 5;
    observed = context->weight;
    pthread_mutex_unlock(&context->weight_mutex);
    pthread_mutex_lock(&context->simulation->mutex);
    context->simulation->demonstration_weight = observed;
    pthread_mutex_unlock(&context->simulation->mutex);
    snprintf(fields, sizeof(fields), "\"id\":%d,\"vehicleType\":\"CAR\",\"vehicleState\":\"CROSSING\",\"weight\":%d,\"message\":\"Reservation enregistree trop tard\"", id, observed);
    events_emit(simulation_generation(context->simulation), "VEHICLE_ENTERED", fields);
    snprintf(fields, sizeof(fields), "\"weight\":%d,\"raceCondition\":true", observed);
    events_emit(simulation_generation(context->simulation), "BRIDGE_STATE", fields);
    simulation_thread_finished(context->simulation);
    return NULL;
}

void scenario_run_race_condition(Simulation *simulation) {
    RaceContext context;
    pthread_t cars[2];
    memset(&context, 0, sizeof(context));
    context.simulation = simulation;
    context.weight = 10;
    context.vehicle_id = 1;
    pthread_mutex_init(&context.weight_mutex, NULL);
    scenario_barrier_init(&context.barrier, 2);
    pthread_mutex_lock(&simulation->mutex);
    simulation->demonstration_weight = 10;
    pthread_mutex_unlock(&simulation->mutex);
    events_emit(simulation_generation(simulation), "BRIDGE_STATE", "\"weight\":10,\"raceCondition\":true");
    scenario_emit_step(simulation, "Check-then-act", "deux lectures synchronisees", "Les deux threads lisent 10 avant toute reservation.");
    pthread_create(&cars[0], NULL, race_vehicle_thread, &context);
    pthread_create(&cars[1], NULL, race_vehicle_thread, &context);
    pthread_join(cars[0], NULL);
    pthread_join(cars[1], NULL);
    events_emit(simulation_generation(simulation), "BRIDGE_COLLAPSED", "\"weight\":20,\"message\":\"Surcharge controlee: absence d'atomicite\"");
    pthread_mutex_lock(&simulation->mutex);
    simulation->demonstration_weight = 20;
    simulation->bridge_collapsed = 1;
    pthread_mutex_unlock(&simulation->mutex);
    scenario_barrier_destroy(&context.barrier);
    pthread_mutex_destroy(&context.weight_mutex);
}
