#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"

#define ALLOCATE(type, count) \
    (type*)Reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) Reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
	((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
	(type*)Reallocate(pointer, sizeof(type) * (oldCount), \
		sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
	Reallocate(pointer, sizeof(type) * (oldCount), 0)

void* Reallocate(void* pointer, size_t oldSize, size_t newSize);
void FreeObjects();

#endif
