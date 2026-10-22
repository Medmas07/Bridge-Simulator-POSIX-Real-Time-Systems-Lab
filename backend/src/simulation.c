#include "simulation.h"

#include "events.h"
#include "scenarios.h"
#include "vehicle.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int milliseconds) { Sleep((DWORD)milliseconds); }
#else
static void sleep_ms(int milliseconds) {
    struct timespec delay = { milliseconds / 1000, (milliseconds % 1000) * 1000000L };
    nanosleep(&delay, NULL);
}
#endif

const char *simulation_state_name(SimulationState state) {
    static const char *names[] = { "IDLE", "RUNNING", "PAUSED", "STOPPING", "FINISHED", "ERROR" };
    return names[state];
}

const char *scenario_name(ScenarioKind scenario) {
    static const char *names[] = { "NORMAL", "RACE_CONDITION", "STARVATION", "DEADLOCK", "BUSY_WAITING", "PRIORITY_INVERSION" };
    return names[scenario];
}

unsigned simulation_generation(Simulation *simulation) {
    unsigned generation;
    pthread_mutex_lock(&simulation->mutex);
    generation = simulation->generation;
    pthread_mutex_unlock(&simulation->mutex);
    return generation;
}

static void emit_thread_count(Simulation *simulation) {
    char fields[128];
    int active;
    unsigned generation;
    pthread_mutex_lock(&simulation->mutex);
    active = simulation->active_threads;
    generation = simulation->generation;
    pthread_mutex_unlock(&simulation->mutex);
    snprintf(fields, sizeof(fields), "\"activeThreads\":%d", active);
    events_emit(generation, "THREAD_COUNT_CHANGED", fields);
}

void simulation_thread_started(Simulation *simulation) {
    pthread_mutex_lock(&simulation->mutex);
    simulation->active_threads++;
    pthread_mutex_unlock(&simulation->mutex);
    emit_thread_count(simulation);
}

void simulation_thread_finished(Simulation *simulation) {
    pthread_mutex_lock(&simulation->mutex);
    if (simulation->active_threads > 0) simulation->active_threads--;
    pthread_mutex_unlock(&simulation->mutex);
    emit_thread_count(simulation);
}

void simulation_vehicle_add(Simulation *simulation, int id, VehicleType type, unsigned generation) {
    pthread_mutex_lock(&simulation->mutex);
    if (simulation->vehicle_count < MAX_VEHICLE_THREADS) {
        VehicleRecord *record = &simulation->vehicles[simulation->vehicle_count++];
        record->id = id;
        record->type = type;
        record->generation = generation;
        snprintf(record->state, sizeof(record->state), "APPEARING");
    }
    pthread_mutex_unlock(&simulation->mutex);
}

void simulation_vehicle_set_state(Simulation *simulation, int id, const char *state) {
    pthread_mutex_lock(&simulation->mutex);
    for (int i = 0; i < simulation->vehicle_count; i++) {
        if (simulation->vehicles[i].id == id) {
            snprintf(simulation->vehicles[i].state, sizeof(simulation->vehicles[i].state), "%s", state);
            break;
        }
    }
    pthread_mutex_unlock(&simulation->mutex);
}

void simulation_vehicle_remove(Simulation *simulation, int id) {
    pthread_mutex_lock(&simulation->mutex);
    for (int i = 0; i < simulation->vehicle_count; i++) {
        if (simulation->vehicles[i].id == id) {
            simulation->vehicles[i] = simulation->vehicles[simulation->vehicle_count - 1];
            simulation->vehicle_count--;
            break;
        }
    }
    pthread_mutex_unlock(&simulation->mutex);
}

int simulation_checkpoint(Simulation *simulation) {
    pthread_mutex_lock(&simulation->mutex);
    while (simulation->state == SIMULATION_PAUSED && !atomic_load(&simulation->stop_requested)) {
        pthread_cond_wait(&simulation->pause_condition, &simulation->mutex);
    }
    int running = !atomic_load(&simulation->stop_requested);
    pthread_mutex_unlock(&simulation->mutex);
    return running;
}

int simulation_wait_ms(Simulation *simulation, int logical_ms) {
    double elapsed = 0.0;
    while (elapsed < logical_ms) {
        double speed;
        if (!simulation_checkpoint(simulation)) return 0;
        pthread_mutex_lock(&simulation->mutex);
        speed = simulation->speed;
        pthread_mutex_unlock(&simulation->mutex);
        sleep_ms(20);
        elapsed += 20.0 * speed;
    }
    return !atomic_load(&simulation->stop_requested);
}

void simulation_snapshot_json(Simulation *simulation, char *output, size_t output_size) {
    BridgeSnapshot bridge = bridge_snapshot(&simulation->bridge);
    unsigned generation;
    ScenarioKind scenario;
    SimulationState state;
    double speed;
    int active;
    int display_weight;
    int collapsed;
    size_t used;

    pthread_mutex_lock(&simulation->mutex);
    generation = simulation->generation;
    scenario = simulation->scenario;
    state = simulation->state;
    speed = simulation->speed;
    active = simulation->active_threads;
    display_weight = scenario == SCENARIO_RACE_CONDITION ? simulation->demonstration_weight : bridge.weight;
    collapsed = simulation->bridge_collapsed;
    snprintf(output, output_size,
             "{\"type\":\"SNAPSHOT\",\"generation\":%u,\"scenario\":\"%s\",\"simulationState\":\"%s\",\"speed\":%.1f,\"weight\":%d,\"activeThreads\":%d,\"waitingCars\":%d,\"waitingTrucks\":%d,\"crossingCars\":%d,\"crossingTrucks\":%d,\"collapsed\":%s,\"vehicles\":[",
             generation, scenario_name(scenario), simulation_state_name(state), speed,
             display_weight, active, bridge.waiting_cars, bridge.waiting_trucks,
             bridge.crossing_cars, bridge.crossing_trucks, collapsed ? "true" : "false");
    used = strlen(output);
    for (int i = 0; i < simulation->vehicle_count && used + 120 < output_size; i++) {
        VehicleRecord *vehicle = &simulation->vehicles[i];
        used += (size_t)snprintf(output + used, output_size - used,
                                 "%s{\"id\":%d,\"vehicleType\":\"%s\",\"vehicleState\":\"%s\"}",
                                 i ? "," : "", vehicle->id,
                                 vehicle->type == VEHICLE_TRUCK ? "TRUCK" : "CAR", vehicle->state);
    }
    snprintf(output + used, output_size - used, "]}");
    pthread_mutex_unlock(&simulation->mutex);
}

void simulation_emit_snapshot(Simulation *simulation) {
    char snapshot[8192];
    simulation_snapshot_json(simulation, snapshot, sizeof(snapshot));
    events_push_raw(snapshot);
}

static void set_state(Simulation *simulation, SimulationState state) {
    unsigned generation;
    char fields[160];
    pthread_mutex_lock(&simulation->mutex);
    simulation->state = state;
    generation = simulation->generation;
    pthread_mutex_unlock(&simulation->mutex);
    snprintf(fields, sizeof(fields), "\"simulationState\":\"%s\"", simulation_state_name(state));
    events_emit(generation, "SIMULATION_STATE_CHANGED", fields);
}

static void *scenario_controller(void *argument) {
    Simulation *simulation = argument;
    ScenarioKind scenario;
    simulation_thread_started(simulation);
    pthread_mutex_lock(&simulation->mutex);
    scenario = simulation->scenario;
    pthread_mutex_unlock(&simulation->mutex);
    switch (scenario) {
        case SCENARIO_NORMAL: scenario_run_normal(simulation); break;
        case SCENARIO_RACE_CONDITION: scenario_run_race_condition(simulation); break;
        case SCENARIO_STARVATION: scenario_run_starvation(simulation); break;
        case SCENARIO_DEADLOCK: scenario_run_deadlock(simulation); break;
        case SCENARIO_BUSY_WAITING: scenario_run_busy_waiting(simulation); break;
        case SCENARIO_PRIORITY_INVERSION: scenario_run_priority_inversion(simulation); break;
    }
    if (!atomic_load(&simulation->stop_requested)) {
        set_state(simulation, SIMULATION_FINISHED);
        events_emit(simulation_generation(simulation), "SCENARIO_FINISHED", "\"message\":\"Demonstration terminee et rejouable\"");
    }
    simulation_thread_finished(simulation);
    return NULL;
}

static void stop_current(Simulation *simulation) {
    int join_controller;
    pthread_t controller;
    /* Toute transition passe ici : le contrôleur et les véhicules sont joints
       avant de réinitialiser le moniteur, sinon un ancien thread pourrait
       encore publier des événements dans la nouvelle génération. */
    pthread_mutex_lock(&simulation->mutex);
    simulation->state = SIMULATION_STOPPING;
    atomic_store(&simulation->stop_requested, true);
    pthread_cond_broadcast(&simulation->pause_condition);
    join_controller = simulation->controller_running;
    controller = simulation->controller_thread;
    pthread_mutex_unlock(&simulation->mutex);
    bridge_wake_all(&simulation->bridge);
    if (join_controller) pthread_join(controller, NULL);
    vehicle_join_all(simulation);
    pthread_mutex_lock(&simulation->mutex);
    simulation->controller_running = 0;
    simulation->active_threads = 0;
    pthread_mutex_unlock(&simulation->mutex);
    bridge_reset(&simulation->bridge);
}

void simulation_init(Simulation *simulation) {
    memset(simulation, 0, sizeof(*simulation));
    pthread_mutex_init(&simulation->mutex, NULL);
    pthread_cond_init(&simulation->pause_condition, NULL);
    bridge_init(&simulation->bridge);
    atomic_init(&simulation->stop_requested, false);
    simulation->generation = 1;
    simulation->scenario = SCENARIO_NORMAL;
    simulation->state = SIMULATION_IDLE;
    simulation->speed = 1.0;
    simulation->next_vehicle_id = 1;
}

void simulation_destroy(Simulation *simulation) {
    stop_current(simulation);
    bridge_destroy(&simulation->bridge);
    pthread_cond_destroy(&simulation->pause_condition);
    pthread_mutex_destroy(&simulation->mutex);
}

int simulation_create_vehicle(Simulation *simulation, VehicleType type) {
    int allowed;
    pthread_mutex_lock(&simulation->mutex);
    allowed = simulation->scenario == SCENARIO_NORMAL &&
              (simulation->state == SIMULATION_RUNNING || simulation->state == SIMULATION_IDLE || simulation->state == SIMULATION_FINISHED);
    if (simulation->state == SIMULATION_IDLE || simulation->state == SIMULATION_FINISHED) simulation->state = SIMULATION_RUNNING;
    pthread_mutex_unlock(&simulation->mutex);
    if (!allowed) {
        events_emit(simulation_generation(simulation), "ERROR", "\"message\":\"Creation manuelle autorisee uniquement en mode NORMAL\"");
        return 0;
    }
    return vehicle_start(simulation, type);
}

void simulation_start_scenario(Simulation *simulation, ScenarioKind scenario) {
    char fields[384];
    stop_current(simulation);
    events_clear();
    pthread_mutex_lock(&simulation->mutex);
    /* Une génération permet au frontend d'ignorer les événements obsolètes. */
    simulation->generation++;
    simulation->scenario = scenario;
    simulation->state = SIMULATION_RUNNING;
    simulation->next_vehicle_id = 1;
    simulation->vehicle_count = 0;
    simulation->demonstration_weight = 0;
    simulation->bridge_collapsed = 0;
    atomic_store(&simulation->stop_requested, false);
    pthread_mutex_unlock(&simulation->mutex);
    snprintf(fields, sizeof(fields), "\"scenario\":\"%s\",\"simulationState\":\"RUNNING\",\"message\":\"Nouveau scenario, generation isolee\"", scenario_name(scenario));
    events_emit(simulation_generation(simulation), "SCENARIO_STARTED", fields);
    simulation_emit_snapshot(simulation);
    if (pthread_create(&simulation->controller_thread, NULL, scenario_controller, simulation) != 0) {
        set_state(simulation, SIMULATION_ERROR);
        events_emit(simulation_generation(simulation), "ERROR", "\"message\":\"Impossible de creer le thread controleur\"");
        return;
    }
    pthread_mutex_lock(&simulation->mutex);
    simulation->controller_running = 1;
    pthread_mutex_unlock(&simulation->mutex);
}

void simulation_reset(Simulation *simulation) {
    stop_current(simulation);
    events_clear();
    pthread_mutex_lock(&simulation->mutex);
    simulation->generation++;
    simulation->scenario = SCENARIO_NORMAL;
    simulation->state = SIMULATION_IDLE;
    simulation->next_vehicle_id = 1;
    simulation->vehicle_count = 0;
    simulation->demonstration_weight = 0;
    simulation->bridge_collapsed = 0;
    atomic_store(&simulation->stop_requested, false);
    pthread_mutex_unlock(&simulation->mutex);
    events_emit(simulation_generation(simulation), "SIMULATION_RESET", "\"weight\":0,\"message\":\"Threads joints, moniteur et files reinitialises\"");
    simulation_emit_snapshot(simulation);
}

void simulation_pause(Simulation *simulation) {
    int changed = 0;
    pthread_mutex_lock(&simulation->mutex);
    if (simulation->state == SIMULATION_RUNNING) { simulation->state = SIMULATION_PAUSED; changed = 1; }
    pthread_mutex_unlock(&simulation->mutex);
    if (changed) set_state(simulation, SIMULATION_PAUSED);
}

void simulation_resume(Simulation *simulation) {
    int changed = 0;
    pthread_mutex_lock(&simulation->mutex);
    if (simulation->state == SIMULATION_PAUSED) { simulation->state = SIMULATION_RUNNING; changed = 1; }
    pthread_cond_broadcast(&simulation->pause_condition);
    pthread_mutex_unlock(&simulation->mutex);
    if (changed) set_state(simulation, SIMULATION_RUNNING);
}

void simulation_set_speed(Simulation *simulation, double speed) {
    char fields[128];
    if (speed != 0.5 && speed != 1.0 && speed != 2.0) return;
    pthread_mutex_lock(&simulation->mutex);
    simulation->speed = speed;
    pthread_mutex_unlock(&simulation->mutex);
    snprintf(fields, sizeof(fields), "\"speed\":%.1f", speed);
    events_emit(simulation_generation(simulation), "SPEED_CHANGED", fields);
}
