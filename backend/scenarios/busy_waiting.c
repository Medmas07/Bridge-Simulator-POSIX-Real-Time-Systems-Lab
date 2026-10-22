#include "scenarios.h"

#include "events.h"
#include "scenario_common.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    Simulation *simulation;
    atomic_bool released;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    unsigned long iterations;
} BusyContext;

static void *busy_loop_thread(void *argument) {
    BusyContext *context = argument;
    simulation_thread_started(context->simulation);
    while (!atomic_load(&context->released) && !atomic_load(&context->simulation->stop_requested)) {
        context->iterations++;
        if ((context->iterations % 10000UL) == 0) sched_yield();
    }
    simulation_thread_finished(context->simulation);
    return NULL;
}

static void *condition_thread(void *argument) {
    BusyContext *context = argument;
    simulation_thread_started(context->simulation);
    pthread_mutex_lock(&context->mutex);
    while (!atomic_load(&context->released) && !atomic_load(&context->simulation->stop_requested)) pthread_cond_wait(&context->condition, &context->mutex);
    pthread_mutex_unlock(&context->mutex);
    simulation_thread_finished(context->simulation);
    return NULL;
}

void scenario_run_busy_waiting(Simulation *simulation) {
    BusyContext context;
    pthread_t busy, condition;
    clock_t start = clock();
    context.simulation = simulation;
    atomic_init(&context.released, false);
    context.iterations = 0;
    pthread_mutex_init(&context.mutex, NULL);
    pthread_cond_init(&context.condition, NULL);
    scenario_emit_step(simulation, "Comparaison", "boucle active vs pthread_cond_wait", "Les deux threads attendent le meme signal pendant une duree bornee.");
    pthread_create(&busy, NULL, busy_loop_thread, &context);
    pthread_create(&condition, NULL, condition_thread, &context);
    simulation_wait_ms(simulation, 2000);
    atomic_store(&context.released, true);
    pthread_mutex_lock(&context.mutex);
    pthread_cond_broadcast(&context.condition);
    pthread_mutex_unlock(&context.mutex);
    pthread_join(busy, NULL);
    pthread_join(condition, NULL);
    char fields[384];
    double cpu_ms = 1000.0 * (double)(clock() - start) / CLOCKS_PER_SEC;
    snprintf(fields, sizeof(fields), "\"checks\":%lu,\"conditionWakeups\":1,\"cpuMs\":%.2f,\"message\":\"La variable de condition ne sonde pas en boucle\"", context.iterations, cpu_ms);
    events_emit(simulation_generation(simulation), "BUSY_WAIT_RESULT", fields);
    pthread_cond_destroy(&context.condition);
    pthread_mutex_destroy(&context.mutex);
}
