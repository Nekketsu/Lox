#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
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

static ObjString* AllocateString(char* chars, int length, uint32_t hash)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    TableSet(&vm.strings, string, NIL_VAL);

    return string;
}

static uint32_t HashString(const char* key, int length)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }

    return hash;
}

ObjString* TakeString(char* chars, int length)
{
    uint32_t hash = HashString(chars, length);
    ObjString* interned = TableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return AllocateString(chars, length, hash);
}

ObjString* CopyString(const char* chars, int length)
{
    uint32_t hash = HashString(chars, length);
    ObjString* interned = TableFindString(&vm.strings, chars, length, hash);
    if (interned  != NULL) { return interned; }

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return AllocateString(heapChars, length, hash);
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
