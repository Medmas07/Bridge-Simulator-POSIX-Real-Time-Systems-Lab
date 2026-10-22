#include "scenarios.h"

#include "events.h"
#include "scenario_common.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Simulation *simulation;
    pthread_mutex_t resource;
    int inheritance_enabled;
} PriorityContext;

static void *priority_low(void *argument) {
    PriorityContext *context = argument;
    simulation_thread_started(context->simulation);
    pthread_mutex_lock(&context->resource);
    events_emit(simulation_generation(context->simulation), "PRIORITY_EVENT", "\"task\":\"T1_LOW\",\"action\":\"LOCKED_MUTEX\"");
    simulation_wait_ms(context->simulation, 2600);
    pthread_mutex_unlock(&context->resource);
    events_emit(simulation_generation(context->simulation), "PRIORITY_EVENT", "\"task\":\"T1_LOW\",\"action\":\"RELEASED_MUTEX\"");
    simulation_thread_finished(context->simulation);
    return NULL;
}

static void *priority_high(void *argument) {
    PriorityContext *context = argument;
    simulation_thread_started(context->simulation);
    simulation_wait_ms(context->simulation, 350);
    events_emit(simulation_generation(context->simulation), "PRIORITY_EVENT", "\"task\":\"T3_HIGH\",\"action\":\"WAITING_MUTEX\"");
    pthread_mutex_lock(&context->resource);
    pthread_mutex_unlock(&context->resource);
    events_emit(simulation_generation(context->simulation), "PRIORITY_EVENT", "\"task\":\"T3_HIGH\",\"action\":\"ACQUIRED_MUTEX\"");
    simulation_thread_finished(context->simulation);
    return NULL;
}

static void *priority_medium(void *argument) {
    PriorityContext *context = argument;
    simulation_thread_started(context->simulation);
    simulation_wait_ms(context->simulation, 550);
    events_emit(simulation_generation(context->simulation), "PRIORITY_EVENT", "\"task\":\"T2_MEDIUM\",\"action\":\"RUNNING_CPU_WORK\"");
    simulation_wait_ms(context->simulation, 1500);
    simulation_thread_finished(context->simulation);
    return NULL;
}

static void run_priority_variant(Simulation *simulation, int inherit) {
    PriorityContext context;
    pthread_mutexattr_t attributes;
    pthread_t low, high, medium;
    memset(&context, 0, sizeof(context));
    context.simulation = simulation;
    pthread_mutexattr_init(&attributes);
#ifdef PTHREAD_PRIO_INHERIT
    if (inherit && pthread_mutexattr_setprotocol(&attributes, PTHREAD_PRIO_INHERIT) == 0) context.inheritance_enabled = 1;
#else
    (void)inherit;
#endif
    pthread_mutex_init(&context.resource, &attributes);
    pthread_mutexattr_destroy(&attributes);
    char fields[320];
    snprintf(fields, sizeof(fields), "\"variant\":\"%s\",\"inheritanceApplied\":%s,\"message\":\"Timeline controlee; le resultat ne prouve pas l'ordonnancement temps reel\"", inherit ? "PRIO_INHERIT" : "NO_INHERIT", context.inheritance_enabled ? "true" : "false");
    events_emit(simulation_generation(simulation), "PRIORITY_VARIANT", fields);
    pthread_create(&low, NULL, priority_low, &context);
    pthread_create(&high, NULL, priority_high, &context);
    pthread_create(&medium, NULL, priority_medium, &context);
    pthread_join(low, NULL);
    pthread_join(high, NULL);
    pthread_join(medium, NULL);
    pthread_mutex_destroy(&context.resource);
}

void scenario_run_priority_inversion(Simulation *simulation) {
    scenario_emit_step(simulation, "Sans heritage", "mutex POSIX standard", "T3 attend une ressource detenue par T1 pendant que T2 travaille.");
    run_priority_variant(simulation, 0);
    if (atomic_load(&simulation->stop_requested)) return;
    scenario_emit_step(simulation, "Avec heritage", "PTHREAD_PRIO_INHERIT", "Le support est teste et annonce explicitement.");
    run_priority_variant(simulation, 1);
}
