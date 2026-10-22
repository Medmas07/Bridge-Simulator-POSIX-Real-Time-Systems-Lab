#ifndef EVENTS_H
#define EVENTS_H
#include "bridge.h"
typedef struct EventNode { char json[512]; struct EventNode *next; } EventNode;
void events_init(void); void events_push(const char *type, int id, VehicleType vehicle, int weight, const char *message); int events_pop(char *out, int size); void events_clear(void);
#endif
