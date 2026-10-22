#include "scenarios.h"

#include "events.h"
#include "scenario_common.h"
#include "vehicle.h"

#include <stdio.h>

void scenario_run_starvation(Simulation *simulation) {
    int logical_waiting_ms = 0;
    scenario_emit_step(simulation, "Occupation initiale", "camion reserve le pont", "Une voiture arrivera pendant la traversee.");
    vehicle_start(simulation, VEHICLE_TRUCK);
    if (!simulation_wait_ms(simulation, 1000)) return;
    vehicle_start(simulation, VEHICLE_CAR);
    events_emit(simulation_generation(simulation), "STARVATION_METRIC", "\"waitingVehicleId\":2,\"trucksServed\":0,\"waitingMs\":0,\"priority\":\"TRUCKS\",\"risk\":true");
    for (int i = 1; i <= 3; i++) {
        char fields[256];
        if (!simulation_wait_ms(simulation, 650)) return;
        logical_waiting_ms += 650;
        vehicle_start(simulation, VEHICLE_TRUCK);
        snprintf(fields, sizeof(fields), "\"waitingVehicleId\":2,\"trucksServed\":%d,\"waitingMs\":%d,\"priority\":\"TRUCKS\",\"risk\":true,\"message\":\"Risque de famine : la demonstration est bornee\"", i, logical_waiting_ms);
        events_emit(simulation_generation(simulation), "STARVATION_METRIC", fields);
    }
    scenario_emit_step(simulation, "File prioritaire", "priorite stricte aux camions", "La demonstration est bornee a trois camions prioritaires.");
    scenario_wait_for_vehicles(simulation);
}
