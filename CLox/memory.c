#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* Reallocate(void* pointer, size_t oldSize, size_t newSize)
{
    vm.bytesAllocated += newSize - oldSize;

    if (newSize > oldSize)
    {
#ifdef DEBUG_STRESS_GC
        CollectGarbage();
#endif

        if (vm.bytesAllocated > vm.nextGC)
        {
            CollectGarbage();
        }
    }

	if (newSize == 0)
	{
		free(pointer);
		return NULL;
	}

	void* result = realloc(pointer, newSize);
	if (result == NULL) { exit(1); }
	return result;
}

void MarkObject(Obj* object)
{
    if (object == NULL) { return; }
    if (object->isMarked) { return; }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    PrintValue(OBJ_VAL(object));
    printf("\n");
#endif
    object->isMarked = true;

    if (vm.grayCapacity < vm.grayCount + 1)
    {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)realloc(vm.grayStack,
                                      sizeof(Obj*) * vm.grayCapacity);

        if (vm.grayStack == NULL) { exit(1); }
    }

    vm.grayStack[vm.grayCount++] = object;
}

void MarkValue(Value value)
{
    if (!IS_OBJ(value)) { return; }
    MarkObject(AS_OBJ(value));
}

static void MarkArray(ValueArray* array)
{
    for (int i = 0; i < array->count; i++)
    {
        MarkValue(array->values[i]);
    }
}

static void BlackenObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    PrintValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type)
    {
        case OBJ_CLASS:
        {
            ObjClass* klass = (ObjClass*)object;
            MarkObject((Obj*)klass->name);
            break;
        }

        case OBJ_CLOSURE:
        {
            ObjClosure* closure = (ObjClosure*)object;
            MarkObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++)
            {
                MarkObject((Obj*)closure->upvalues[i]);
            }
            break;
        }

        case OBJ_FUNCTION:
            ObjFunction* function = (ObjFunction*)object;
            MarkObject((Obj*)function->name);
            MarkArray(&function->chunk.constants);
            break;

        case OBJ_INSTANCE:
        {
            ObjInstance* instance = (ObjInstance*)object;
            MarkObject((Obj*)instance->klass);
            MarkTable(&instance->fields);
            break;
        }

        case OBJ_UPVALUE:
            MarkValue(((ObjUpvalue*)object)->closed);
            break;

        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void FreeObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type)
    {
        case OBJ_CLASS:
        {
            FREE(ObjClass, object);
            break;
        }

        case OBJ_CLOSURE:
        {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }

        case OBJ_FUNCTION:
        {
            ObjFunction* function = (ObjFunction*)object;
            FreeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }

        case OBJ_INSTANCE:
        {
            ObjInstance* instance = (ObjInstance*)object;
            FreeTable(&instance->fields);
            FREE(ObjInstance, object);
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

static void MarkRoots()
{
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
    {
        MarkValue(*slot);
    }

    for (int i = 0; i < vm.frameCount; i++)
    {
        MarkObject((Obj*)vm.frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm.openUpvalues;
         upvalue != NULL;
         upvalue = upvalue->next)
    {
        MarkObject((Obj*)upvalue);
    }

    MarkTable(&vm.globals);
    MarkCompilerRoots();
}

static void TraceReferences()
{
    while (vm.grayCount > 0)
    {
        Obj* object = vm.grayStack[--vm.grayCount];
        BlackenObject(object);
    }
}

static void Sweep()
{
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object != NULL)
    {
        if (object->isMarked)
        {
            object->isMarked = false;
            previous = object;
            object = object->next;
        }
        else
        {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL)
            {
                previous->next = object;
            }
            else
            {
                vm.objects = object;
            }

            FreeObject(unreached);
        }
    }
}

void CollectGarbage()
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    MarkRoots();
    TraceReferences();
    TableRemoveWhite(&vm.strings);
    Sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytesAllocated, before, vm.bytesAllocated,
           vm.nextGC);
#endif
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

    free(vm.grayStack);
}
