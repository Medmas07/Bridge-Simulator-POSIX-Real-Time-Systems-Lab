#ifndef SCENARIO_COMMON_H
#define SCENARIO_COMMON_H

#include "simulation.h"

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int arrived;
    int target;
    int phase;
} ScenarioBarrier;

void scenario_barrier_init(ScenarioBarrier *barrier, int target);
void scenario_barrier_wait(ScenarioBarrier *barrier);
void scenario_barrier_destroy(ScenarioBarrier *barrier);
void scenario_emit_step(Simulation *simulation, const char *step, const char *posix_event, const char *explanation);
void scenario_wait_for_vehicles(Simulation *simulation);

#endif
