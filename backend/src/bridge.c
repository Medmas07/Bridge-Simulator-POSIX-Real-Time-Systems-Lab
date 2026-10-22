#include "bridge.h"
#include <assert.h>
void bridge_init(Bridge *b) { pthread_mutex_init(&b->mutex, 0); pthread_cond_init(&b->cars, 0); pthread_cond_init(&b->trucks, 0); b->weight=0; b->waiting_cars=0; b->waiting_trucks=0; b->crossing_cars=0; }
void bridge_destroy(Bridge *b) { pthread_cond_destroy(&b->cars); pthread_cond_destroy(&b->trucks); pthread_mutex_destroy(&b->mutex); }
void bridge_acquire(Bridge *b, VehicleType type) {
  int w = type == VEHICLE_TRUCK ? 15 : 5;
  pthread_mutex_lock(&b->mutex);
  if (type == VEHICLE_TRUCK) b->waiting_trucks++; else b->waiting_cars++;
  while (type == VEHICLE_TRUCK ? (b->weight != 0) : (b->weight + w > BRIDGE_CAPACITY || b->waiting_trucks > 0)) pthread_cond_wait(type == VEHICLE_TRUCK ? &b->trucks : &b->cars, &b->mutex);
  if (type == VEHICLE_TRUCK) b->waiting_trucks--; else b->waiting_cars--;
  b->weight += w; if (type == VEHICLE_CAR) b->crossing_cars++;
  assert(b->weight <= BRIDGE_CAPACITY); pthread_mutex_unlock(&b->mutex);
}
void bridge_release(Bridge *b, VehicleType type) { pthread_mutex_lock(&b->mutex); b->weight -= type == VEHICLE_TRUCK ? 15 : 5; if (type == VEHICLE_CAR) b->crossing_cars--; assert(b->weight >= 0); if (b->waiting_trucks) pthread_cond_signal(&b->trucks); else pthread_cond_broadcast(&b->cars); pthread_mutex_unlock(&b->mutex); }
int bridge_weight(Bridge *b) { int value; pthread_mutex_lock(&b->mutex); value=b->weight; pthread_mutex_unlock(&b->mutex); return value; }
