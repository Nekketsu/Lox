using System;
using System.Globalization;
using System.IO;

namespace CraftingInterpreters.Lox
{
    public class Lox
    {
        public static CultureInfo CultureInfo { get; } = new CultureInfo("en-US");
        private static readonly Interpreter interpreter = new Interpreter();

        static bool hadError = false;
        static bool hadRuntimeError = false;

        static int Main(string[] args)
        {
            if (args.Length > 1)
            {
                Console.WriteLine("Usage: Lox [script]");
                System.Environment.Exit(64);
            }
            else if (args.Length == 1)
            {
                RunFile(args[0]);
            }
            else
            {
                RunPrompt();
            }

            return 0;
        }

        private static void RunFile(string path)
        {
            var source = File.ReadAllText(path);

            Run(source);

            // Indicate an error in the exit code
            if (hadError) { System.Environment.Exit(65); }
            if (hadRuntimeError) { System.Environment.Exit(70); }
        }
        
        private static void RunPrompt()
        {
            while (true)
            {
                Console.Write("> ");
                var line = Console.ReadLine();
                if (line == null) { break; }
                Run(line);
                hadError = false;
            }
        }

        private static void Run(string source)
        {
            var scanner = new Scanner(source);
            var tokens = scanner.ScanTokens();
            var parser = new Parser(tokens);
            var statements = parser.Parse();

            // Stop if there was a syntax error.
            if (hadError) { return; }

            var resolver = new Resolver(interpreter);
            resolver.Resolve(statements);

            // Stop if there was a resolution error.
            if (hadError) { return; }

            interpreter.Interpret(statements);
        }

        public static void Error(int line, string message)
        {
            Report(line, "", message);
        }

        private static void Report(int line, string where, string message)
        {
            Console.Error.WriteLine($"[line {line}] Error{where}: {message}");
            hadError = true;
        }

        public static void Error(Token token, string message)
        {
            if (token.Type == TokenType.EOF)
            {
                Report(token.Line, " at end", message);
            }
            else
            {
                Report(token.Line, $" at '{token.Lexeme}'", message);
            }
        }

        public static void RuntimeError(RuntimeError error)
        {
            Console.Error.WriteLine($"{error.Message}\n[line {error.Token.Line}]");
            hadRuntimeError = true;
        }
    }
}
