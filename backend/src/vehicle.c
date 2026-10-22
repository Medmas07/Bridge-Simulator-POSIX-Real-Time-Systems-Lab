#include "vehicle.h"

#include "events.h"
#include "simulation.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    APPEARANCE_MS = 500,
    APPROACH_MS = 3000,
    CROSSING_MS = 7000,
    EXIT_MS = 2000
};

static const char *vehicle_type_name(VehicleType type) {
    return type == VEHICLE_TRUCK ? "TRUCK" : "CAR";
}

static int vehicle_weight(VehicleType type) {
    return type == VEHICLE_TRUCK ? 15 : 5;
}

static void emit_vehicle(Vehicle *vehicle, const char *event_type, const char *state, const char *message) {
    char fields[768];
    snprintf(fields, sizeof(fields),
             "\"id\":%d,\"vehicleType\":\"%s\",\"vehicleState\":\"%s\",\"vehicleWeight\":%d,\"message\":\"%s\"",
             vehicle->id, vehicle_type_name(vehicle->type), state,
             vehicle_weight(vehicle->type), message);
    simulation_vehicle_set_state(vehicle->simulation, vehicle->id, state);
    events_emit(vehicle->generation, event_type, fields);
}

static void emit_bridge_state(Vehicle *vehicle) {
    BridgeSnapshot bridge = bridge_snapshot(&vehicle->simulation->bridge);
    char fields[512];
    snprintf(fields, sizeof(fields),
             "\"weight\":%d,\"waitingCars\":%d,\"waitingTrucks\":%d,\"crossingCars\":%d,\"crossingTrucks\":%d",
             bridge.weight, bridge.waiting_cars, bridge.waiting_trucks,
             bridge.crossing_cars, bridge.crossing_trucks);
    events_emit(vehicle->generation, "BRIDGE_STATE", fields);
}

static void finish_vehicle(Vehicle *vehicle, const char *state, const char *message) {
    emit_vehicle(vehicle, "VEHICLE_FINISHED", state, message);
    simulation_vehicle_remove(vehicle->simulation, vehicle->id);
    simulation_thread_finished(vehicle->simulation);
    free(vehicle);
}

static void *vehicle_thread(void *argument) {
    Vehicle *vehicle = argument;
    Simulation *simulation = vehicle->simulation;
    simulation_thread_started(simulation);
    emit_vehicle(vehicle, "VEHICLE_CREATED", "APPEARING", "Thread POSIX cree");

    if (!simulation_wait_ms(simulation, APPEARANCE_MS)) {
        finish_vehicle(vehicle, "CANCELLED", "Annule pendant l'apparition");
        return NULL;
    }

    emit_vehicle(vehicle, "VEHICLE_ARRIVING", "ARRIVING", "Approche de la barriere");
    if (!simulation_wait_ms(simulation, APPROACH_MS)) {
        finish_vehicle(vehicle, "CANCELLED", "Annule pendant l'approche");
        return NULL;
    }

    emit_vehicle(vehicle, "VEHICLE_WAITING", "WAITING", "Attend l'autorisation du moniteur");
    events_emit(vehicle->generation, "MONITOR_EVENT",
                "\"monitorState\":\"LOCKING\",\"message\":\"pthread_mutex_lock puis verification de la condition\"");
    emit_bridge_state(vehicle);

    if (!bridge_acquire_interruptible(&simulation->bridge, vehicle->type, &simulation->stop_requested)) {
        emit_bridge_state(vehicle);
        finish_vehicle(vehicle, "CANCELLED", "Reveille pour arret de la simulation");
        return NULL;
    }

    emit_vehicle(vehicle, "VEHICLE_ENTERED", "CROSSING", "Admission accordee par le moniteur");
    events_emit(vehicle->generation, "MONITOR_EVENT",
                "\"monitorState\":\"UNLOCKED\",\"message\":\"Reservation terminee, mutex libere pendant la traversee\"");
    emit_bridge_state(vehicle);

    (void)simulation_wait_ms(simulation, CROSSING_MS);
    bridge_release(&simulation->bridge, vehicle->type);
    emit_vehicle(vehicle, "VEHICLE_EXITED", "EXITING", "Pont libere apres 7 secondes logiques");
    emit_bridge_state(vehicle);

    (void)simulation_wait_ms(simulation, EXIT_MS);
    finish_vehicle(vehicle, "FINISHED", "Vehicule sorti de la scene");
    return NULL;
}

int vehicle_start(Simulation *simulation, VehicleType type) {
    Vehicle *vehicle;
    pthread_t thread;
    int index;

    pthread_mutex_lock(&simulation->mutex);
    if (atomic_load(&simulation->stop_requested) || simulation->vehicle_thread_count >= MAX_VEHICLE_THREADS) {
        pthread_mutex_unlock(&simulation->mutex);
        return 0;
    }
    vehicle = calloc(1, sizeof(*vehicle));
    if (!vehicle) {
        pthread_mutex_unlock(&simulation->mutex);
        return 0;
    }
    vehicle->id = simulation->next_vehicle_id++;
    vehicle->type = type;
    vehicle->generation = simulation->generation;
    vehicle->simulation = simulation;
    if (simulation->vehicle_count < MAX_VEHICLE_THREADS) {
        VehicleRecord *record = &simulation->vehicles[simulation->vehicle_count++];
        record->id = vehicle->id;
        record->type = type;
        record->generation = vehicle->generation;
        snprintf(record->state, sizeof(record->state), "APPEARING");
    }
    index = simulation->vehicle_thread_count;
    if (pthread_create(&thread, NULL, vehicle_thread, vehicle) != 0) {
        simulation->vehicle_count--;
        free(vehicle);
        pthread_mutex_unlock(&simulation->mutex);
        return 0;
    }
    simulation->vehicle_threads[index] = thread;
    simulation->vehicle_thread_count++;
    pthread_mutex_unlock(&simulation->mutex);
    return vehicle->id;
}

void vehicle_join_all(Simulation *simulation) {
    int count;
    pthread_t threads[MAX_VEHICLE_THREADS];
    pthread_mutex_lock(&simulation->mutex);
    count = simulation->vehicle_thread_count;
    for (int i = 0; i < count; i++) threads[i] = simulation->vehicle_threads[i];
    simulation->vehicle_thread_count = 0;
    pthread_mutex_unlock(&simulation->mutex);
    for (int i = 0; i < count; i++) pthread_join(threads[i], NULL);
}
