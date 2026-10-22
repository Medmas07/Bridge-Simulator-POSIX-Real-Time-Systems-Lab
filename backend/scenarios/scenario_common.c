#include "scenario_common.h"

#include "events.h"

#include <stdio.h>

void scenario_barrier_init(ScenarioBarrier *barrier, int target) {
    pthread_mutex_init(&barrier->mutex, NULL);
    pthread_cond_init(&barrier->condition, NULL);
    barrier->arrived = 0;
    barrier->target = target;
    barrier->phase = 0;
}

void scenario_barrier_wait(ScenarioBarrier *barrier) {
    int phase;
    pthread_mutex_lock(&barrier->mutex);
    phase = barrier->phase;
    barrier->arrived++;
    /* Le while est obligatoire avec pthread_cond_wait : un réveil ne
       signifie pas nécessairement que la condition est satisfaite. */
    if (barrier->arrived == barrier->target) {
        barrier->arrived = 0;
        barrier->phase++;
        pthread_cond_broadcast(&barrier->condition);
    } else {
        while (phase == barrier->phase) pthread_cond_wait(&barrier->condition, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
}

void scenario_barrier_destroy(ScenarioBarrier *barrier) {
    pthread_cond_destroy(&barrier->condition);
    pthread_mutex_destroy(&barrier->mutex);
}

void scenario_emit_step(Simulation *simulation, const char *step, const char *posix_event, const char *explanation) {
    char fields[768];
    snprintf(fields, sizeof(fields), "\"step\":\"%s\",\"posixEvent\":\"%s\",\"explanation\":\"%s\"", step, posix_event, explanation);
    events_emit(simulation_generation(simulation), "SCENARIO_STEP", fields);
    simulation_wait_for_step(simulation);
}

void scenario_wait_for_vehicles(Simulation *simulation) {
    simulation_wait_ms(simulation, 250);
    for (;;) {
        int active;
        pthread_mutex_lock(&simulation->mutex);
        active = simulation->active_threads;
        pthread_mutex_unlock(&simulation->mutex);
        if (active <= 1 || atomic_load(&simulation->stop_requested)) break;
        if (!simulation_wait_ms(simulation, 100)) break;
    }
}
