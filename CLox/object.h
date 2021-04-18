#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_STRING(value)       IsObjType(value, OBJ_STRING)

#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum
{
	OBJ_STRING
} ObjType;

struct Obj
{
	ObjType type;
    struct Obj* next;
};

struct ObjString
{
	Obj obj;
	int length;
	char* chars;
};

ObjString* TakeString(char* chars, int length);
ObjString* CopyString(const char* chars, int length);
void PrintObject(Value value);

static inline bool IsObjType(Value value, ObjType type)
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif

