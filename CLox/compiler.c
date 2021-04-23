#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct
{
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

typedef enum
{
	PREC_NONE,
	PREC_ASSIGNMENT,  // =
	PREC_OR,		  // or
	PREC_AND,		  // and
	PREC_EQUALITY,	  // == !=
	PREC_COMPARISON,  // < > <= >=
	PREC_TERM,		  // + -
	PREC_FACTOR,	  // * /
	PREC_UNARY,		  // ! -
	PREC_CALL,		  // . ()
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct
{
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct
{
    Token name;
    int depth;
} Local;

typedef struct
{
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

Parser parser;

Compiler* current = NULL;

Chunk* compilingChunk;

static Chunk* CurrentChunk()
{
	return compilingChunk;
}

static void ErrorAt(Token* token, const char* message)
{
	if (parser.panicMode) { return; }
	parser.panicMode = true;

	fprintf(stderr, "[line %d] Error", token->line);

	if (token->type == TOKEN_EOF)
	{
		fprintf(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR)
	{
		// Nothing.
	}
	else
	{
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

static void Error(const char* message)
{
	ErrorAt(&parser.previous, message);
}

static void ErrorAtCurrent(const char* message)
{
	ErrorAt(&parser.current, message);
}

static void Advance()
{
	parser.previous = parser.current;

	for (;;)
	{
		parser.current = ScanToken();
		if (parser.current.type != TOKEN_ERROR) { break; }

		ErrorAtCurrent(parser.current.start);
	}
}

static void Consume(TokenType type, const char* message)
{
	if (parser.current.type == type)
	{
		Advance();
		return;
	}

	ErrorAtCurrent(message);
}

static bool Check(TokenType type)
{
    return parser.current.type == type;
}

static bool Match(TokenType type)
{
    if (!Check(type)) { return false; }
    Advance();
    return true;
}

static void EmitByte(uint8_t byte)
{
	WriteChunk(CurrentChunk(), byte, parser.previous.line);
}

static void EmitBytes(uint8_t byte1, uint8_t byte2)
{
    EmitByte(byte1);
    EmitByte(byte2);
}

static void EmitLoop(int loopStart)
{
    EmitByte(OP_LOOP);

    int offset = CurrentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) { Error("loop body too large."); }

    EmitByte((offset >> 8) & 0xff);
    EmitByte(offset & 0xff);
}

static int EmitJump(uint8_t instruction)
{
    EmitByte(instruction);
    EmitByte(0xff);
    EmitByte(0xff);
    return CurrentChunk()->count - 2;
}

static void EmitReturn()
{
	EmitByte(OP_RETURN);
}

static uint8_t MakeConstant(Value value)
{
	int constant = AddConstant(CurrentChunk(), value);
	if (constant > UINT8_MAX)
	{
		Error("Too many constants in one chunk.");
		return 0;
	}

	return (uint8_t)constant;
}

static void EmitConstant(Value value)
{
	EmitBytes(OP_CONSTANT, MakeConstant(value));
}

static void PatchJump(int offset)
{
    // -2 adjust for the bytecode for the jump offset itself.
    int jump = CurrentChunk()->count - offset - 2;

    if (jump > UINT16_MAX)
    {
        Error("Too much code to jump over.");
    }

    CurrentChunk()->code[offset] = (jump >> 8) & 0xff;
    CurrentChunk()->code[offset + 1] = jump & 0xff;
}

static void InitCompiler(Compiler* compiler)
{
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

static void EndCompiler()
{
	EmitReturn();
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError)
	{
		DisassembleChunk(CurrentChunk(), "code");
	}
#endif
}

static void BeginScope()
{
    current->scopeDepth++;
}

static void EndScope()
{
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
               current->scopeDepth)
    {
        EmitByte(OP_POP);
        current->localCount--;
    }
}

static void Expression();
static void Statement();
static void Declaration();
static ParseRule* GetRule(TokenType type);
static void ParsePrecedence(Precedence precedence);

static uint8_t IdentifierConstant(Token* name)
{
    return MakeConstant(OBJ_VAL(CopyString(name->start, name->length)));
}

static bool IdentifiersEqual(Token* a, Token* b)
{
    if (a->length != b->length) { return false; }
    return memcmp(a->start, b->start, a->length) == 0;
}

static int ResolveLocal(Compiler* compiler, Token* name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local* local = &compiler->locals[i];
        if (IdentifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                Error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void AddLocal(Token name)
{
    if (current->localCount == UINT8_COUNT)
    {
        Error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

static void DeclareVariable()
{
    if (current->scopeDepth == 0) { return; }

    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--)
    {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
        {
            break;
        }

        if (IdentifiersEqual(name, &local->name))
        {
            Error("Already variable with this name in this scope.");
        }
    }

    AddLocal(*name);
}

static uint8_t ParseVariable(const char* errorMessage)
{
    Consume(TOKEN_IDENTIFIER, errorMessage);

    DeclareVariable();
    if (current->scopeDepth > 0) { return 0; }

    return IdentifierConstant(&parser.previous);
}

static void MarkInitialized()
{
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void DefineVariable(uint8_t global)
{
    if (current->scopeDepth > 0)
    {
        MarkInitialized();
        return;
    }

    EmitBytes(OP_DEFINE_GLOBAL, global);
}

static void And_(bool canAssign)
{
    int endJump = EmitJump(OP_JUMP_IF_FALSE);

    EmitByte(OP_POP);
    ParsePrecedence(PREC_AND);

    PatchJump(endJump);
}

static void Binary(bool canAssign)
{
	// Remember the operator.
	TokenType operatorType = parser.previous.type;

	// Compile the right operand.
	ParseRule* rule = GetRule(operatorType);
	ParsePrecedence((Precedence)(rule->precedence + 1));

	// Emit the operator instruction.
	switch (operatorType)
	{
		case TOKEN_BANG_EQUAL:    EmitBytes(OP_EQUAL, OP_NOT); break;
		case TOKEN_EQUAL_EQUAL:   EmitByte(OP_EQUAL); break;
		case TOKEN_GREATER:       EmitByte(OP_GREATER); break;
		case TOKEN_GREATER_EQUAL: EmitBytes(OP_LESS, OP_NEGATE); break;
		case TOKEN_LESS:          EmitByte(OP_LESS); break;
		case TOKEN_LESS_EQUAL:    EmitBytes(OP_GREATER, OP_NOT); break;
		case TOKEN_PLUS:          EmitByte(OP_ADD); break;
		case TOKEN_MINUS:         EmitByte(OP_SUBTRACT); break;
		case TOKEN_STAR:          EmitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH:         EmitByte(OP_DIVIDE); break;
		default:
			return; // Unreachable.
	}
}

static void Literal(bool canAssign)
{
	switch (parser.previous.type)
	{
		case TOKEN_FALSE: EmitByte(OP_FALSE); break;
		case TOKEN_NIL: EmitByte(OP_NIL); break;
		case TOKEN_TRUE: EmitByte(OP_TRUE); break;
		default:
			return; // Unreachable.
	}
}

static void Grouping(bool canAssign)
{
	Expression();
	Consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void Number(bool canAssign)
{
	double value = strtod(parser.previous.start, NULL);
	EmitConstant(NUMBER_VAL(value));
}

static void Or_(bool canAssign)
{
    int elseJump = EmitJump(OP_JUMP_IF_FALSE);
    int endJump = EmitJump(OP_JUMP);

    PatchJump(elseJump);
    EmitByte(OP_POP);

    ParsePrecedence(PREC_OR);
    PatchJump(endJump);
}

static void String(bool canAssign)
{
    EmitConstant(OBJ_VAL(CopyString(parser.previous.start + 1,
                                    parser.previous.length - 2)));
}

static void NamedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = ResolveLocal(current, &name);
    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else
    {
        arg = IdentifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && Match(TOKEN_EQUAL))
    {
        Expression();
        EmitBytes(setOp, arg);
    }
    else
    {
        EmitBytes(getOp, arg);
    }
}

static void Variable(bool canAssign)
{
    NamedVariable(parser.previous, canAssign);
}

static void Unary(bool canAssign)
{
	TokenType operatorType = parser.previous.type;

	// Compile the operand.
	ParsePrecedence(PREC_UNARY);

	// Emit the oeprator instruction.
	switch (operatorType)
	{
        case TOKEN_BANG: EmitByte(OP_NOT); break;
            case TOKEN_MINUS: EmitByte(OP_NEGATE); break;
            default:
                return; // Unreachable.
	}
}

ParseRule rules[] =
{
	[TOKEN_LEFT_PAREN]    = { Grouping, NULL,   PREC_NONE },
	[TOKEN_RIGHT_PAREN]   = { NULL,     NULL,   PREC_NONE },
	[TOKEN_LEFT_BRACE]    = { NULL,     NULL,   PREC_NONE },
	[TOKEN_RIGHT_BRACE]   = { NULL,     NULL,   PREC_NONE },
	[TOKEN_COMMA]         = { NULL,     NULL,   PREC_NONE },
	[TOKEN_DOT]           = { NULL,     NULL,   PREC_NONE },
	[TOKEN_MINUS]         = { Unary,    Binary, PREC_TERM },
	[TOKEN_PLUS]          = { NULL,     Binary, PREC_TERM },
	[TOKEN_SEMICOLON]     = { NULL,     NULL,   PREC_NONE },
	[TOKEN_SLASH]         = { NULL,     Binary, PREC_FACTOR },
	[TOKEN_STAR]          = { NULL,     Binary, PREC_FACTOR },
	[TOKEN_BANG]          = { Unary,    NULL,   PREC_NONE },
	[TOKEN_BANG_EQUAL]    = { NULL,     Binary, PREC_EQUALITY },
	[TOKEN_EQUAL]         = { NULL,     NULL,   PREC_NONE },
	[TOKEN_EQUAL_EQUAL]   = { NULL,     Binary, PREC_EQUALITY },
	[TOKEN_GREATER]       = { NULL,     Binary, PREC_COMPARISON },
	[TOKEN_GREATER_EQUAL] = { NULL,     Binary, PREC_COMPARISON },
	[TOKEN_LESS]          = { NULL,     Binary, PREC_COMPARISON },
	[TOKEN_LESS_EQUAL]    = { NULL,     Binary, PREC_COMPARISON },
	[TOKEN_IDENTIFIER]    = { Variable, NULL,   PREC_NONE },
	[TOKEN_STRING]        = { String,   NULL,   PREC_NONE },
	[TOKEN_NUMBER]        = { Number,   NULL,   PREC_NONE },
	[TOKEN_AND]           = { NULL,     And_,   PREC_AND },
	[TOKEN_CLASS]         = { NULL,     NULL,   PREC_NONE },
	[TOKEN_ELSE]          = { NULL,     NULL,   PREC_NONE },
	[TOKEN_FALSE]         = { Literal,  NULL,   PREC_NONE },
	[TOKEN_FOR]           = { NULL,     NULL,   PREC_NONE },
	[TOKEN_FUN]           = { NULL,     NULL,   PREC_NONE },
	[TOKEN_IF]            = { NULL,     NULL,   PREC_NONE },
	[TOKEN_NIL]           = { Literal,  NULL,   PREC_NONE },
	[TOKEN_OR]            = { NULL,     Or_,    PREC_OR },
	[TOKEN_PRINT]         = { NULL,     NULL,   PREC_NONE },
	[TOKEN_RETURN]        = { NULL,     NULL,   PREC_NONE },
	[TOKEN_SUPER]         = { NULL,     NULL,   PREC_NONE },
	[TOKEN_THIS]          = { NULL,     NULL,   PREC_NONE },
	[TOKEN_TRUE]          = { Literal,  NULL,   PREC_NONE },
	[TOKEN_VAR]           = { NULL,     NULL,   PREC_NONE },
	[TOKEN_WHILE]         = { NULL,     NULL,   PREC_NONE },
	[TOKEN_ERROR]         = { NULL,     NULL,   PREC_NONE },
	[TOKEN_EOF]           = { NULL,     NULL,   PREC_NONE }
};

static void ParsePrecedence(Precedence precedence)
{
	Advance();
	ParseFn prefixRule = GetRule(parser.previous.type)->prefix;
	if (prefixRule == NULL)
	{
		Error("Expected expression.");
		return;
	}

    bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence <= GetRule(parser.current.type)->precedence)
	{
		Advance();
		ParseFn infixRule = GetRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

    if (canAssign && Match(TOKEN_EQUAL))
    {
        Error("Invalid assignment target.");
    }
}

static ParseRule* GetRule(TokenType type)
{
	return &rules[type];
}

static void Expression()
{
	ParsePrecedence(PREC_ASSIGNMENT);
}

static void Block()
{
    while (!Check(TOKEN_RIGHT_BRACE) && !Check(TOKEN_EOF))
    {
        Declaration();
    }

    Consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void VarDeclaration()
{
	uint8_t global = ParseVariable("Expect variable name.");

	if (Match(TOKEN_EQUAL))
	{
		Expression();
	}
	else
	{
		EmitByte(OP_NIL);
	}
	Consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	DefineVariable(global);
}

static void ExpressionStatement()
{
    Expression();
    Consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    EmitByte(OP_POP);
}

static void ForStatement()
{
    BeginScope();

    Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (Match(TOKEN_SEMICOLON))
    {
        // No initializer.
    }
    else if (Match(TOKEN_VAR))
    {
        VarDeclaration();
    }
    else
    {
        ExpressionStatement();
    }

    int loopStart = CurrentChunk()->count;

    int exitJump = -1;
    if (!Match(TOKEN_SEMICOLON))
    {
        Expression();
        Consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = EmitJump(OP_JUMP_IF_FALSE);
        EmitByte(OP_POP); // Condition.
    }

    if (!Match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = EmitJump(OP_JUMP);

        int incrementStart = CurrentChunk()->count;
        Expression();
        EmitByte(OP_POP);
        Consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        EmitLoop(loopStart);
        loopStart = incrementStart;
        PatchJump(bodyJump);
    }

    Statement();

    EmitLoop(loopStart);

    if (exitJump != -1)
    {
        PatchJump(exitJump);
        EmitByte(OP_POP); // Condition.
    }

    EndScope();
}

static void IfStatement()
{
    Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    Expression();
    Consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = EmitJump(OP_JUMP_IF_FALSE);
    EmitByte(OP_POP);
    Statement();

    int elseJump = EmitJump(OP_JUMP);

    PatchJump(thenJump);
    EmitByte(OP_POP);

    if (Match(TOKEN_ELSE)) { Statement(); }
    PatchJump(elseJump);
}

static void PrintStatement()
{
    Expression();
    Consume(TOKEN_SEMICOLON, "Expect ';? after value.");
    EmitByte(OP_PRINT);
}

static void WhileStatement()
{
    int loopStart = CurrentChunk()->count;

    Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    Expression();
    Consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = EmitJump(OP_JUMP_IF_FALSE);

    EmitByte(OP_POP);
    Statement();

    EmitLoop(loopStart);

    PatchJump(exitJump);
    EmitByte(OP_POP);
}

static void Synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON) { return; }

        switch (parser.current.type)
        {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                // Do nothing.
                ;
        }

        Advance();
    }
}

static void Declaration()
{
    if (Match(TOKEN_VAR))
    {
        VarDeclaration();
    }
    else
    {
        Statement();
    }

    if (parser.panicMode) { Synchronize(); }
}

static void Statement()
{
    if (Match(TOKEN_PRINT))
    {
        PrintStatement();
    }
    else if (Match(TOKEN_FOR))
    {
        ForStatement();
    }
    else if (Match(TOKEN_IF))
    {
        IfStatement();
    }
    else if (Match(TOKEN_WHILE))
    {
        WhileStatement();
    }
    else if (Match(TOKEN_LEFT_BRACE))
    {
        BeginScope();
        Block();
        EndScope();
    }
    else
    {
        ExpressionStatement();
    }
}

bool Compile(const char* source, Chunk* chunk)
{
	InitScanner(source);
    Compiler compiler;
    InitCompiler(&compiler);
	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	Advance();

    while (!Match(TOKEN_EOF))
    {
        Declaration();
    }

	EndCompiler();
	return !parser.hadError;
}
