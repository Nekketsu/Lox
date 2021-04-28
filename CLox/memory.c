#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void* Reallocate(void* pointer, size_t oldSize, size_t newSize)
{
	if (newSize == 0)
	{
		free(pointer);
		return NULL;
	}

	void* result = realloc(pointer, newSize);
	if (result == NULL) { exit(1); }
	return result;
}

static void FreeObject(Obj* object)
{
    switch (object->type)
    {
        case OBJ_CLOSURE:
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;

        case OBJ_FUNCTION:
        {
            ObjFunction* function = (ObjFunction*)object;
            FreeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }

        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;

        case OBJ_STRING:
        {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }

        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
    }
}

void FreeObjects()
{
    Obj* object = vm.objects;
    while (object != NULL)
    {
        Obj* next = object->next;
        FreeObject(object);
        object = next;
    }
}
