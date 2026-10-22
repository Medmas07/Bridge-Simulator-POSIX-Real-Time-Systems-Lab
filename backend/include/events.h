#ifndef EVENTS_H
#define EVENTS_H

#include <stddef.h>

typedef struct EventNode {
    char json[8192];
    struct EventNode *next;
} EventNode;

void events_init(void);
void events_destroy(void);
void events_clear(void);
void events_push_raw(const char *json);
void events_emit(unsigned generation, const char *type, const char *fields_json);
int events_pop(char *output, size_t output_size);

#endif
