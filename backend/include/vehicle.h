#ifndef VEHICLE_H
#define VEHICLE_H
#include "bridge.h"
typedef struct { int id; VehicleType type; Bridge *bridge; } Vehicle;
void vehicle_start(Bridge *bridge, VehicleType type);
#endif
