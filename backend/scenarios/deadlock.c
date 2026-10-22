#include "scenarios.h"

#include "events.h"
#include "scenario_common.h"

#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <stdbool.h>

typedef struct {
    Simulation *simulation;
    ScenarioBarrier barrier;
    pthread_mutex_t barrier_mutex;
    pthread_mutex_t balance_mutex;
} DeadlockContext;

static void *deadlock_first(void *argument) {
    DeadlockContext *context = argument;
    simulation_thread_started(context->simulation);
    pthread_mutex_lock(&context->barrier_mutex);
    events_emit(simulation_generation(context->simulation), "RESOURCE_STATE", "\"thread\":\"T1\",\"owns\":\"mutex_barriere\",\"waitsFor\":\"mutex_balance\"");
    scenario_barrier_wait(&context->barrier);
    while (!atomic_load(&context->simulation->stop_requested) && pthread_mutex_trylock(&context->balance_mutex) == EBUSY) { }
    if (!atomic_load(&context->simulation->stop_requested)) pthread_mutex_unlock(&context->balance_mutex);
    pthread_mutex_unlock(&context->barrier_mutex);
    simulation_thread_finished(context->simulation);
    return NULL;
}

static void *deadlock_second(void *argument) {
    DeadlockContext *context = argument;
    simulation_thread_started(context->simulation);
    pthread_mutex_lock(&context->balance_mutex);
    events_emit(simulation_generation(context->simulation), "RESOURCE_STATE", "\"thread\":\"T2\",\"owns\":\"mutex_balance\",\"waitsFor\":\"mutex_barriere\"");
    scenario_barrier_wait(&context->barrier);
    while (!atomic_load(&context->simulation->stop_requested) && pthread_mutex_trylock(&context->barrier_mutex) == EBUSY) { }
    if (!atomic_load(&context->simulation->stop_requested)) pthread_mutex_unlock(&context->barrier_mutex);
    pthread_mutex_unlock(&context->balance_mutex);
    simulation_thread_finished(context->simulation);
    return NULL;
}

void scenario_run_deadlock(Simulation *simulation) {
    DeadlockContext context;
    pthread_t first, second;
    context.simulation = simulation;
    pthread_mutex_init(&context.barrier_mutex, NULL);
    pthread_mutex_init(&context.balance_mutex, NULL);
    scenario_barrier_init(&context.barrier, 2);
    pthread_create(&first, NULL, deadlock_first, &context);
    pthread_create(&second, NULL, deadlock_second, &context);
    simulation_wait_ms(simulation, 1200);
    events_emit(simulation_generation(simulation), "DEADLOCK_DETECTED", "\"cycle\":\"T1 -> T2 -> T1\",\"message\":\"Interblocage controle par trylock, arret toujours possible\"");
    simulation_wait_ms(simulation, 1800);
    atomic_store(&simulation->stop_requested, true);
    pthread_join(first, NULL);
    pthread_join(second, NULL);
    atomic_store(&simulation->stop_requested, false);
    scenario_barrier_destroy(&context.barrier);
    pthread_mutex_destroy(&context.barrier_mutex);
    pthread_mutex_destroy(&context.balance_mutex);
}
