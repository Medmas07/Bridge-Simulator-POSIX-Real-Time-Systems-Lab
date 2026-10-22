#include "events.h"
#include "simulation.h"

void websocket_server_run(Simulation *simulation);

int main(void) {
    Simulation simulation;
    events_init();
    simulation_init(&simulation);
    websocket_server_run(&simulation);
    simulation_destroy(&simulation);
    events_destroy();
    return 0;
}
