#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int arg, char* argv[])
{
	Chunk chunk;
	InitChunk(&chunk);

	int constant = AddConstant(&chunk, 1.2);
	WriteChunk(&chunk, OP_CONSTANT, 123);
	WriteChunk(&chunk, constant, 123);

	WriteChunk(&chunk, OP_RETURN, 123);

	DisassembleChunk(&chunk, "test chunk");
	FreeChunk(&chunk);
	return 0;
}