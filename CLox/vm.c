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
    vm.openUpvalues = NULL;
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
        ObjFunction* function = frame->closure->function;
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
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    InitTable(&vm.globals);
    InitTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = CopyString("init", 4);

    DefineNative("clock", ClockNative);
}

void FreeVM()
{
    FreeTable(&vm.globals);
    FreeTable(&vm.strings);
    vm.initString = NULL;
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

static bool Call(ObjClosure* closure, int argCount)
{
    if (argCount != closure->function->arity)
    {
        RuntimeError("Expected %d arguments but got %d.",
                closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX)
    {
        RuntimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;

    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool CallValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            case OBJ_BOUND_METHOD:
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argCount - 1] = bound->receiver;
                return Call(bound->method, argCount);
            case OBJ_CLASS:
            {
                ObjClass* klass = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = OBJ_VAL(NewInstance(klass));
                Value initializer;
                if (TableGet(&klass->methods, vm.initString, &initializer))
                {
                    return Call(AS_CLOSURE(initializer), argCount);
                }
                else if (argCount != 0)
                {
                    RuntimeError("Expected 0 arguments but got %d.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return Call(AS_CLOSURE(callee), argCount);

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

static bool InvokeFromClass(ObjClass* klass, ObjString* name, int argCount)
{
    Value method;
    if (!TableGet(&klass->methods, name, &method))
    {
        RuntimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    return Call(AS_CLOSURE(method), argCount);
}

static bool Invoke(ObjString* name, int argCount)
{
    Value receiver = Peek(argCount);

    if (!IS_INSTANCE(receiver))
    {
        RuntimeError("Only instances have methods.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);

    Value value;
    if (TableGet(&instance->fields, name, &value))
    {
        vm.stackTop[-argCount - 1] = value;
        return CallValue(value, argCount);
    }

    return InvokeFromClass(instance->klass, name, argCount);
}

static bool BindMethod(ObjClass* klass, ObjString* name)
{
    Value method;
    if (!TableGet(&klass->methods, name, &method))
    {
        RuntimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = NewBoundMethod(Peek(0), AS_CLOSURE(method));

    Pop();
    Push(OBJ_VAL(bound));
    return true;
}

static ObjUpvalue* CaptureUpvalue(Value* local)
{
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local)
    {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = NewUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL)
    {
        vm.openUpvalues = createdUpvalue;
    }
    else
    {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void CloseUpvalues(Value* last)
{
    while (vm.openUpvalues != NULL &&
           vm.openUpvalues->location >= last)
    {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static void DefineMethod(ObjString* name)
{
    Value method = Peek(0);
    ObjClass* klass = AS_CLASS(Peek(1));
    TableSet(&klass->methods, name, method);
    Pop();
}

static bool IsFalsey(Value value)
{
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void Concatenate()
{
    ObjString* b = AS_STRING(Peek(0));
    ObjString* a = AS_STRING(Peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = TakeString(chars, length);
    Pop();
    Pop();
    Push(OBJ_VAL(result));
}

static InterpretResult Run()
{
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
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

		DisassembleInstruction(&frame->closure->function->chunk,
                (int)(frame->ip - frame->closure->function->chunk.code));
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

            case OP_GET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                Push(*frame->closure->upvalues[slot]->location);
                break;
            }

            case OP_SET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = Peek(0);
                break;
            }

            case OP_GET_PROPERTY:
            {
                if (!IS_INSTANCE(Peek(0)))
                {
                    RuntimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(Peek(0));
                ObjString* name = READ_STRING();

                Value value;
                if (TableGet(&instance->fields, name, &value))
                {
                    Pop(); // Instance.
                    Push(value);
                    break;
                }

                if (!BindMethod(instance->klass, name))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SET_PROPERTY:
            {
                if (!IS_INSTANCE(Peek(1)))
                {
                    RuntimeError("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(Peek(1));
                TableSet(&instance->fields, READ_STRING(), Peek(0));

                Value value = Pop();
                Pop();
                Push(value);
                break;
            }

            case OP_GET_SUPER:
            {
                ObjString* name = READ_STRING();
                ObjClass* superClass = AS_CLASS(Pop());
                if (!BindMethod(superClass, name))
                {
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
                    RuntimeError(
                        "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
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

            case OP_INVOKE:
            {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                if (!Invoke(method, argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }

            case OP_SUPER_INVOKE:
            {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(Pop());
                if (!InvokeFromClass(superclass, method, argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }

            case OP_CLOSURE:
            {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = NewClosure(function);
                Push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++)
                {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal)
                    {
                        closure->upvalues[i] =
                            CaptureUpvalue(frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }

            case OP_CLOSE_UPVALUE:
                CloseUpvalues(vm.stackTop - 1);
                Pop();
                break;

			case OP_RETURN:
			{
                Value result = Pop();

                CloseUpvalues(frame->slots);

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

            case OP_CLASS:
                Push(OBJ_VAL(NewClass(READ_STRING())));
                break;
            case OP_INHERIT:
                Value superclass = Peek(1);
                if (!IS_CLASS(superclass))
                {
                    RuntimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass* subclass = AS_CLASS(Peek(0));
                TableAddAll(&AS_CLASS(superclass)->methods,
                            &subclass->methods);
                Pop(); // Subclass.
                break;
            case OP_METHOD:
                DefineMethod(READ_STRING());
                break;
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
    ObjClosure* closure = NewClosure(function);
    Pop();
    Push(OBJ_VAL(closure));
    CallValue(OBJ_VAL(closure), 0);

    return Run();
}
