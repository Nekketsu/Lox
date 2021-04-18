#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)AllocateObject(sizeof(type), objectType);

static Obj* AllocateObject(size_t size, ObjType type)
{
    Obj* object = (Obj*)Reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static ObjString* AllocateString(char* chars, int length)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;

    return string;
}

ObjString* TakeString(char* chars, int lenght)
{
    return AllocateString(chars, lenght);
}

ObjString* CopyString(const char* chars, int length)
{
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return AllocateString(heapChars, length);
}

void PrintObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}
