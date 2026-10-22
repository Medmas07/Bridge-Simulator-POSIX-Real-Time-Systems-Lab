#ifndef SIMULATION_H
#define SIMULATION_H

#include "bridge.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>

#define MAX_VEHICLE_THREADS 64

typedef enum {
    SCENARIO_NORMAL,
    SCENARIO_RACE_CONDITION,
    SCENARIO_STARVATION,
    SCENARIO_DEADLOCK,
    SCENARIO_BUSY_WAITING,
    SCENARIO_PRIORITY_INVERSION
} ScenarioKind;

typedef enum {
    SIMULATION_IDLE,
    SIMULATION_RUNNING,
    SIMULATION_PAUSED,
    SIMULATION_STOPPING,
    SIMULATION_FINISHED,
    SIMULATION_ERROR
} SimulationState;

typedef struct {
    int id;
    VehicleType type;
    char state[20];
    unsigned generation;
} VehicleRecord;

typedef struct Simulation {
    pthread_mutex_t mutex;
    pthread_cond_t pause_condition;
    Bridge bridge;
    atomic_bool stop_requested;
    unsigned generation;
    ScenarioKind scenario;
    SimulationState state;
    double speed;
    int next_vehicle_id;
    int active_threads;
    pthread_t vehicle_threads[MAX_VEHICLE_THREADS];
    int vehicle_thread_count;
    pthread_t controller_thread;
    int controller_running;
    VehicleRecord vehicles[MAX_VEHICLE_THREADS];
    int vehicle_count;
    int demonstration_weight;
    int bridge_collapsed;
} Simulation;

void simulation_init(Simulation *simulation);
void simulation_destroy(Simulation *simulation);
int simulation_create_vehicle(Simulation *simulation, VehicleType type);
void simulation_start_scenario(Simulation *simulation, ScenarioKind scenario);
void simulation_reset(Simulation *simulation);
void simulation_pause(Simulation *simulation);
void simulation_resume(Simulation *simulation);
void simulation_set_speed(Simulation *simulation, double speed);
int simulation_wait_ms(Simulation *simulation, int logical_ms);
int simulation_checkpoint(Simulation *simulation);
void simulation_snapshot_json(Simulation *simulation, char *output, size_t output_size);
void simulation_emit_snapshot(Simulation *simulation);
void simulation_thread_started(Simulation *simulation);
void simulation_thread_finished(Simulation *simulation);
void simulation_vehicle_add(Simulation *simulation, int id, VehicleType type, unsigned generation);
void simulation_vehicle_set_state(Simulation *simulation, int id, const char *state);
void simulation_vehicle_remove(Simulation *simulation, int id);
unsigned simulation_generation(Simulation *simulation);
const char *simulation_state_name(SimulationState state);
const char *scenario_name(ScenarioKind scenario);

#endif
