#ifndef clox_compiler_h
#define clox_compiler_h


#include "vm.h"
bool Compile(const char* source, Chunk* chunk);

#endif
