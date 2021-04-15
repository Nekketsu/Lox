#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

VM vm;

static void ResetStack()
{
	vm.stackTop = vm.stack;
}

static void RuntimeError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	size_t instruction = vm.ip - vm.chunk->code - 1;
	int line = vm.chunk->lines[instruction];
	fprintf(stderr, "[line %d] in script\n", line);

	ResetStack();
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

static Value Peek(int distance)
{
	return vm.stackTop[-1 - distance];
}

static bool IsFalsey(Value value)
{
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static InterpretResult Run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

#define BINARY_OP(valueType, op) \
	do { \
		if (!IS_NUMBER(Peek(0)) || !IS_NUMBER(Peek(1))) \
		{ \
			RuntimeError("Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		} \
		double b = AS_NUMBER(Pop()); \
		double a = AS_NUMBER(Pop()); \
		Push(valueType(a op b)); \
	} while (false)

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
			case OP_NIL: Push(NIL_VAL); break;
			case OP_TRUE: Push(BOOL_VAL(true)); break;
			case OP_FALSE: Push(BOOL_VAL(false)); break;

			case OP_EQUAL:
			{
				Value b = Pop();
				Value a = Pop();
				Push(BOOL_VAL(ValuesEqual(a, b)));
				break;
			}

			case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
			case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
			case OP_ADD:	  BINARY_OP(NUMBER_VAL, +); break;
			case OP_SUBTRACT: BINARY_OP(NUMBER_VAL,  -); break;
			case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIVIDE:	  BINARY_OP(NUMBER_VAL, /); break;
			case OP_NOT:
				Push(BOOL_VAL(IsFalsey(Pop())));
				break;
			case OP_NEGATE:
				if (!IS_NUMBER(Peek(0)))
				{
					RuntimeError("Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}

				Push(NUMBER_VAL(-AS_NUMBER(Pop())));
				break;
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

InterpretResult Interpret(const char* source)
{
	Chunk chunk;
	InitChunk(&chunk);

	if (!Compile(source, &chunk))
	{
		FreeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;

	InterpretResult result = Run();

	FreeChunk(&chunk);
	return result;
}