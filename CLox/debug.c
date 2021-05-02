#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"

void DisassembleChunk(Chunk* chunk, const char* name)
{
	printf("== %s ==\n", name);

	for (int offset = 0; offset < chunk->count;)
	{
		offset = DisassembleInstruction(chunk, offset);
	}
}

static int ConstantInstruction(const char* name, Chunk* chunk, int offset)
{
	uint8_t constant = chunk->code[offset + 1];
	printf("%-16s %4d '", name, constant);
	PrintValue(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 2;
}

static int InvokeInstruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, argCount, constant);
    PrintValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

static int SimpleInstruction(const char* name, int offset)
{
	printf("%s\n", name);
	return offset + 1;
}

static int ByteInstruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int JumpInstruction(const char* name, int sign,
                           Chunk* chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    
    return offset + 3;
}

int DisassembleInstruction(Chunk* chunk, int offset)
{
	printf("%04d ", offset);
	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
	{
		printf("   | ");
	}
	else
	{
		printf("%4d ", chunk->lines[offset]);
	}

	uint8_t instruction = chunk->code[offset];
	switch (instruction)
	{
		case OP_CONSTANT:
			return ConstantInstruction("OP_CONSTANT", chunk, offset);
		case OP_NIL:
			return SimpleInstruction("OP_NIL", offset);
		case OP_TRUE:
			return SimpleInstruction("OP_TRUE", offset);
		case OP_FALSE:
			return SimpleInstruction("OP_FALSE", offset);
        case OP_POP:
            return SimpleInstruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return ByteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return ByteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return ConstantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return ConstantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return ConstantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_UPVALUE:
            return ByteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return ByteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OP_GET_PROPERTY:
            return ConstantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY:
            return ConstantInstruction("OP_SET_PROPERTY", chunk, offset);
        case OP_GET_SUPER:
            return ConstantInstruction("OP_GET_SUPER", chunk, offset);
		case OP_EQUAL:
			return SimpleInstruction("OP_EQUAL", offset);
		case OP_GREATER:
			return SimpleInstruction("OP_GREATER", offset);
		case OP_LESS:
			return SimpleInstruction("OP_LESS", offset);
		case OP_ADD:
			return SimpleInstruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return SimpleInstruction("OP_SUBTRACT", offset);
		case OP_MULTIPLY:
			return SimpleInstruction("OP_MULTIPLY", offset);
		case OP_DIVIDE:
			return SimpleInstruction("OP_DIVIDE", offset);
		case OP_NOT:
			return SimpleInstruction("OP_NOT", offset);
		case OP_NEGATE:
			return SimpleInstruction("OP_NEGATE", offset);
        case OP_PRINT:
            return SimpleInstruction("OP_PRINT", offset);
        case OP_JUMP:
            return JumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return JumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return JumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return ByteInstruction("OP_CALL", chunk, offset);
        case OP_INVOKE:
            return InvokeInstruction("OP_INVOKE", chunk, offset);
        case OP_SUPER_INVOKE:
            return InvokeInstruction("OP_SUPER_INVOKE", chunk, offset);
        case OP_CLOSURE:
        {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            PrintValue(chunk->constants.values[constant]);
            printf("\n");

            ObjFunction* function = AS_FUNCTION(
                chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalueCount; j++)
            {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n",
                    offset - 2, isLocal ? "local" : "upvalue", index);
            }

            return offset;
        }
        case OP_CLOSE_UPVALUE:
            return SimpleInstruction("OP_CLOSE_UPVALUE", offset);
		case OP_RETURN:
			return SimpleInstruction("OP_RETURN", offset);
        case OP_CLASS:
            return ConstantInstruction("OP_CLASS", chunk, offset);
        case OP_INHERIT:
            return SimpleInstruction("OP_INHERIT", offset);
        case OP_METHOD:
            return ConstantInstruction("OP_METHOD", chunk, offset);
		default:
			printf("Unknown opcode %d\n", instruction);
			return offset + 1;
	}
}
