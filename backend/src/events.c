#include "events.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EventNode *head;
static EventNode *tail;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned long sequence;

void events_init(void) {
    pthread_mutex_lock(&queue_mutex);
    head = NULL;
    tail = NULL;
    sequence = 0;
    pthread_mutex_unlock(&queue_mutex);
}

void events_push_raw(const char *json) {
    EventNode *node = calloc(1, sizeof(*node));
    if (!node) return;
    snprintf(node->json, sizeof(node->json), "%s", json);
    pthread_mutex_lock(&queue_mutex);
    if (tail) tail->next = node;
    else head = node;
    tail = node;
    pthread_mutex_unlock(&queue_mutex);
}

void events_emit(unsigned generation, const char *type, const char *fields_json) {
    EventNode *node = calloc(1, sizeof(*node));
    if (!node) return;
    pthread_mutex_lock(&queue_mutex);
    sequence++;
    snprintf(node->json, sizeof(node->json),
             "{\"type\":\"%s\",\"sequence\":%lu,\"generation\":%u%s%s}",
             type, sequence, generation,
             fields_json && fields_json[0] ? "," : "",
             fields_json && fields_json[0] ? fields_json : "");
    if (tail) tail->next = node;
    else head = node;
    tail = node;
    pthread_mutex_unlock(&queue_mutex);
}

int events_pop(char *output, size_t output_size) {
    EventNode *node;
    pthread_mutex_lock(&queue_mutex);
    node = head;
    if (!node) {
        pthread_mutex_unlock(&queue_mutex);
        return 0;
    }
    head = node->next;
    if (!head) tail = NULL;
    pthread_mutex_unlock(&queue_mutex);
    snprintf(output, output_size, "%s", node->json);
    free(node);
    return 1;
}

void events_clear(void) {
    char ignored[8192];
    while (events_pop(ignored, sizeof(ignored))) { }
}

void events_destroy(void) {
    events_clear();
    pthread_mutex_destroy(&queue_mutex);
}
