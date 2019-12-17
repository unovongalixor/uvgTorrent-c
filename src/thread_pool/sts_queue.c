#include "sts_queue.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct StsElement {
  void *next;
  void *value;
} StsElement;

struct StsHeader {
  StsElement *head;
  StsElement *tail;
  pthread_mutex_t mutex;
};

static StsHeader* create();
static StsHeader* create() {
  StsHeader *handle = malloc(sizeof(*handle));
  handle->head = NULL;
  handle->tail = NULL;

  pthread_mutex_init(&(handle->mutex), NULL);

  return handle;
}

static void destroy(StsHeader *header);
static void destroy(StsHeader *header) {
  pthread_mutex_destroy(&(header->mutex));
  free(header);
  header = NULL;
}

static void push(StsHeader *header, void *elem);
static void push(StsHeader *header, void *elem) {
  // Create new element
  StsElement *element = malloc(sizeof(*element));
  element->value = elem;
  element->next = NULL;

  pthread_mutex_lock(&(header->mutex));
  // Is list empty
  if (header->head == NULL) {
	header->head = element;
	header->tail = element;
  } else {
	// Rewire
	StsElement* oldTail = header->tail;
	oldTail->next = element;
	header->tail = element;
  }
  pthread_mutex_unlock(&(header->mutex));
}

static void* pop(StsHeader *header);
static void* pop(StsHeader *header) {
  pthread_mutex_lock(&(header->mutex));
  StsElement *head = header->head;

  // Is empty?
  if (head == NULL) {
	pthread_mutex_unlock(&(header->mutex));
	return NULL;
  } else {
	// Rewire
	header->head = head->next;

	// Get head and free element memory
	void *value = head->value;
	free(head);

	pthread_mutex_unlock(&(header->mutex));
	return value;
  }
}

_StsQueue const StsQueue = {
  create,
  destroy,
  push,
  pop
};
