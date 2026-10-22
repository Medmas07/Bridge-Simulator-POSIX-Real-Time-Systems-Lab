#include "scenarios.h"

#include "scenario_common.h"
#include "vehicle.h"

void scenario_run_normal(Simulation *simulation) {
    scenario_emit_step(simulation, "1/4 - Trois voitures", "pthread_create x3", "Trois voitures peuvent reserver 5 tonnes chacune.");
    vehicle_start(simulation, VEHICLE_CAR);
    vehicle_start(simulation, VEHICLE_CAR);
    vehicle_start(simulation, VEHICLE_CAR);
    if (!simulation_wait_ms(simulation, 1000)) return;

    scenario_emit_step(simulation, "2/4 - Camion prioritaire", "pthread_cond_wait(camions)", "Le camion attend que le pont soit vide.");
    vehicle_start(simulation, VEHICLE_TRUCK);
    if (!simulation_wait_ms(simulation, 250)) return;

    scenario_emit_step(simulation, "3/4 - Voitures bloquees", "pthread_cond_wait(voitures)", "Les nouvelles voitures cedent la priorite au camion en attente.");
    vehicle_start(simulation, VEHICLE_CAR);
    vehicle_start(simulation, VEHICLE_CAR);
    scenario_emit_step(simulation, "4/4 - Ordre controle", "pthread_cond_signal / broadcast", "Camion seul, puis reveil des voitures restantes.");
    scenario_wait_for_vehicles(simulation);
}
