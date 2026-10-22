#ifndef VEHICLE_H
#define VEHICLE_H

#include "bridge.h"

struct Simulation;

typedef struct {
    int id;
    VehicleType type;
    unsigned generation;
    struct Simulation *simulation;
} Vehicle;

int vehicle_start(struct Simulation *simulation, VehicleType type);
void vehicle_join_all(struct Simulation *simulation);

#endif
