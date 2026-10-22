#ifndef SCENARIOS_H
#define SCENARIOS_H

#include "simulation.h"

void scenario_run_normal(Simulation *simulation);
void scenario_run_race_condition(Simulation *simulation);
void scenario_run_starvation(Simulation *simulation);
void scenario_run_deadlock(Simulation *simulation);
void scenario_run_busy_waiting(Simulation *simulation);
void scenario_run_priority_inversion(Simulation *simulation);

#endif
