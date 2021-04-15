#include <stdio.h>
#include <stdlib.h>

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

typedef void (*ParseFn)();

typedef struct
{
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

Parser parser;


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

static void EmitByte(uint8_t byte)
{
	WriteChunk(CurrentChunk(), byte, parser.previous.line);
}

static void EmitBytes(uint8_t byte1, uint8_t byte2)
{
    EmitByte(byte1);
    EmitByte(byte2);
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

static void Expression();
static ParseRule* GetRule(TokenType type);
static void ParsePrecedence(Precedence precedence);

static void Binary()
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
		case TOKEN_LESS_EQUAL:    EmitByte(OP_GREATER, OP_NOT); break;
		case TOKEN_PLUS:          EmitByte(OP_ADD); break;
		case TOKEN_MINUS:         EmitByte(OP_SUBTRACT); break;
		case TOKEN_STAR:          EmitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH:         EmitByte(OP_DIVIDE); break;
		default:
			return; // Unreachable.
	}
}

static void Literal()
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

static void Grouping()
{
	Expression();
	Consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void Number()
{
	double value = strtod(parser.previous.start, NULL);
	EmitConstant(NUMBER_VAL(value));
}

static void Unary()
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
	[TOKEN_IDENTIFIER]    = { NULL,     NULL,   PREC_NONE },
	[TOKEN_STRING]        = { NULL,     NULL,   PREC_NONE },
	[TOKEN_NUMBER]        = { Number,   NULL,   PREC_NONE },
	[TOKEN_AND]           = { NULL,     NULL,   PREC_NONE },
	[TOKEN_CLASS]         = { NULL,     NULL,   PREC_NONE },
	[TOKEN_ELSE]          = { NULL,     NULL,   PREC_NONE },
	[TOKEN_FALSE]         = { Literal,  NULL,   PREC_NONE },
	[TOKEN_FOR]           = { NULL,     NULL,   PREC_NONE },
	[TOKEN_FUN]           = { NULL,     NULL,   PREC_NONE },
	[TOKEN_IF]            = { NULL,     NULL,   PREC_NONE },
	[TOKEN_NIL]           = { Literal,  NULL,   PREC_NONE },
	[TOKEN_OR]            = { NULL,     NULL,   PREC_NONE },
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

	prefixRule();

	while (precedence <= GetRule(parser.current.type)->precedence)
	{
		Advance();
		ParseFn infixRule = GetRule(parser.previous.type)->infix;
		infixRule();
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

bool Compile(const char* source, Chunk* chunk)
{
	InitScanner(source);
	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	Advance();
	Expression();
	Consume(TOKEN_EOF, "Expect end of expression.");
	EndCompiler();
	return !parser.hadError;
}
