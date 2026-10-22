#include "bridge.h"
#include "events.h"
void websocket_server_run(Bridge *bridge);
int main(void){Bridge bridge; bridge_init(&bridge); events_init(); websocket_server_run(&bridge); bridge_destroy(&bridge); return 0;}
