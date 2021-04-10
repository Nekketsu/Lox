#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int arg, char* argv[])
{
	InitVM();

	Chunk chunk;
	InitChunk(&chunk);

	int constant = AddConstant(&chunk, 1.2);
	WriteChunk(&chunk, OP_CONSTANT, 123);
	WriteChunk(&chunk, constant, 123);

	constant = AddConstant(&chunk, 3.4);
	WriteChunk(&chunk, OP_CONSTANT, 123);
	WriteChunk(&chunk, constant, 123);

	WriteChunk(&chunk, OP_ADD, 123);

	constant = AddConstant(&chunk, 5.6);
	WriteChunk(&chunk, OP_CONSTANT, 123);
	WriteChunk(&chunk, constant, 123);

	WriteChunk(&chunk, OP_DIVIDE, 123);
	WriteChunk(&chunk, OP_NEGATE, 123);

	WriteChunk(&chunk, OP_RETURN, 123);

	DisassembleChunk(&chunk, "test chunk");
	Interpret(&chunk);
	FreeVM();
	FreeChunk(&chunk);
	return 0;
}