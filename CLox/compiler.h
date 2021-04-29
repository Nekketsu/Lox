#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

ObjFunction* Compile(const char* source);
void MarkCompilerRoots();

#endif
