#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

VM vm;

static void ResetStack()
{
	vm.stackTop = vm.stack;
}

void InitVM()
{
	ResetStack();
}

void FreeVM()
{

}

void Push(Value value)
{
	*vm.stackTop = value;
	vm.stackTop++;
}

Value Pop()
{
	vm.stackTop--;
	return *vm.stackTop;
}

static InterpretResult Run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

#define BINARY_OP(op) \
	do { \
		double b = Pop(); \
		double a = Pop(); \
		Push(a op b); \
	} while (false);

	for (;;)
	{
#ifdef DEBUG_TRACE_EXECUTION
		printf("          ");
		for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
		{
			printf("[ ");
			PrintValue(*slot);
			printf(" ]");
		}
		printf("\n");

		DisassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t intruction;
		switch (intruction = READ_BYTE())
		{
			case OP_CONSTANT:
			{
				Value constant = READ_CONSTANT();
				Push(constant);
				break;
			}
			case OP_ADD:	  BINARY_OP(+); break;
			case OP_SUBTRACT: BINARY_OP(-); break;
			case OP_MULTIPLY: BINARY_OP(*); break;
			case OP_DIVIDE:	  BINARY_OP(/); break;
			case OP_NEGATE:   Push(-Pop()); break;
			case OP_RETURN:
			{
				PrintValue(Pop());
				printf("\n");
				return INTERPRET_OK;
			}
		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult Interpret(Chunk* chunk)
{
	vm.chunk = chunk;
	vm.ip = vm.chunk->code;
	return Run();
}