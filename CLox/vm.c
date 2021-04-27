#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

VM vm;

static Value ClockNative(int argCount, Value* args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void ResetStack()
{
	vm.stackTop = vm.stack;
    vm.frameCount = 0;
}

static void RuntimeError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--)
    {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        // -1 because the IP is sitting on the next instruction to be
        // executed.
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ",
                function->chunk.lines[instruction]);
        if (function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

	ResetStack();
}

static void DefineNative(const char* name, NativeFn function)
{
    Push(OBJ_VAL(CopyString(name, (int)strlen(name))));
    Push(OBJ_VAL(NewNative(function)));
    TableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    Pop();
    Pop();
}

void InitVM()
{
	ResetStack();
    vm.objects = NULL;

    InitTable(&vm.globals);
    InitTable(&vm.strings);

    DefineNative("clock", ClockNative);
}

void FreeVM()
{
    FreeTable(&vm.globals);
    FreeTable(&vm.strings);
    FreeObjects();
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

static bool Call(ObjFunction* function, int argCount)
{
    if (argCount != function->arity)
    {
        RuntimeError("Expected %d arguments but got %d.",
                function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX)
    {
        RuntimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;

    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool CallValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            case OBJ_FUNCTION:
                return Call(AS_FUNCTION(callee), argCount);

            case OBJ_NATIVE:
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                Push(result);
                return true;

            default:
                // Non-callable object type.
                break;
        }
    }

    RuntimeError("Can only call functions and classes.");
    return false;
}

static bool IsFalsey(Value value)
{
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void Concatenate()
{
    ObjString* b = AS_STRING(Pop());
    ObjString* a = AS_STRING(Pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = TakeString(chars, length);
    Push(OBJ_VAL(result));
}

static InterpretResult Run()
{
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, \
     (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

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

		DisassembleInstruction(&frame->function->chunk,
                (int)(frame->ip - frame->function->chunk.code));
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
            case OP_POP: Pop(); break;

            case OP_GET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                Push(frame->slots[slot]);
                break;
            }

            case OP_SET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = Peek(0);
                break;
            }

            case OP_GET_GLOBAL:
            {
                ObjString* name = READ_STRING();
                Value value;
                if (!TableGet(&vm.globals, name, &value))
                {
                    RuntimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Push(value);
                break;
            }

            case OP_DEFINE_GLOBAL:
            {
                ObjString* name = READ_STRING();
                TableSet(&vm.globals, name, Peek(0));
                Pop();
                break;
            }

            case OP_SET_GLOBAL:
            {
                ObjString* name = READ_STRING();
                if (TableSet(&vm.globals, name, Peek(0)))
                {
                    TableDelete(&vm.globals, name);
                    RuntimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

			case OP_EQUAL:
			{
				Value b = Pop();
				Value a = Pop();
				Push(BOOL_VAL(ValuesEqual(a, b)));
				break;
			}

			case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
			case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
			case OP_ADD:
            {
                if (IS_STRING(Peek(0)) && IS_STRING(Peek(1)))
                {
                    Concatenate();
                }
                else if (IS_NUMBER(Peek(0)) && IS_NUMBER(Peek(1)))
                {
                    double b = AS_NUMBER(Pop());
                    double a = AS_NUMBER(Pop());
                    Push(NUMBER_VAL(a + b));
                }
                else
                {
                    RuntimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            BINARY_OP(NUMBER_VAL, +); break;
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

            case OP_PRINT:
            {
                PrintValue(Pop());
                printf("\n");
                break;
            }

            case OP_JUMP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }

            case OP_JUMP_IF_FALSE:
            {
                uint16_t offset = READ_SHORT();
                if (IsFalsey(Peek(0))) { frame->ip += offset; }
                break;
            }

            case OP_LOOP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

            case OP_CALL:
            {
                int argCount = READ_BYTE();
                if (!CallValue(Peek(argCount), argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }

			case OP_RETURN:
			{
                Value result = Pop();

                vm.frameCount--;
                if (vm.frameCount == 0)
                {
                    Pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                Push(result);

                frame = &vm.frames[vm.frameCount - 1];
                break;
			}
		}
	}

#undef READ_BYTE
#undef READ_SHORT
#undef READ_STRING
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult Interpret(const char* source)
{
    ObjFunction* function = Compile(source);
    if (function == NULL) { return INTERPRET_COMPILE_ERROR; }

    Push(OBJ_VAL(function));
    CallValue(OBJ_VAL(function), 0);

    return Run();
}
