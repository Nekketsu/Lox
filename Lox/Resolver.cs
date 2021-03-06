using System.Collections.Generic;
using System.Linq;

namespace CraftingInterpreters.Lox
{
    public class Resolver : Expr.Visitor<object>, Stmt.Visitor<object>
    {
        private readonly Interpreter interpreter;
        private readonly Stack<Dictionary<string, bool>> scopes = new Stack<Dictionary<string, bool>>();
        private FunctionType currentFunction = FunctionType.NONE;

        public Resolver(Interpreter interpreter)
        {
            this.interpreter = interpreter;
        }

        private enum FunctionType
        {
            NONE,
            FUNCTION,
            INITIALIZER,
            METHOD
        }

        private enum ClassType
        {
            NONE,
            CLASS,
            SUBCLASS
        }

        private ClassType currentClass = ClassType.NONE;

        public void Resolve(Stmt[] statements)
        {
            foreach (var statement in statements)
            {
                Resolve(statement);
            }
        }

        public object VisitBlockStmt(Stmt.Block stmt)
        {
            BeginScope();
            Resolve(stmt.Statements);
            EndScope();
            return null;
        }

        public object VisitClassStmt(Stmt.Class stmt)
        {
            var enclosingClass = currentClass;
            currentClass = ClassType.CLASS;

            Declare(stmt.Name);
            Define(stmt.Name);

            if (stmt.Superclass != null && stmt.Name.Lexeme == stmt.Superclass.Name.Lexeme)
            {
                Lox.Error(stmt.Superclass.Name, "A class can't inherit from itself.");
            }

            if (stmt.Superclass != null)
            {
                currentClass = ClassType.SUBCLASS;
                Resolve(stmt.Superclass);
            }

            if (stmt.Superclass != null)
            {
                BeginScope();
                scopes.Peek()["super"] = true;
            }

            BeginScope();
            scopes.Peek()["this"] = true;

            foreach (var method in stmt.Methods)
            {
                var declaration = FunctionType.METHOD;
                if (method.Name.Lexeme == "init")
                {
                    declaration = FunctionType.INITIALIZER;
                }
                ResolveFunction(method, declaration);
            }

            EndScope();

            if (stmt.Superclass != null) { EndScope(); }

            currentClass = enclosingClass;
            return null;
        }

        public object VisitExpressionStmt(Stmt.Expression stmt)
        {
            Resolve(stmt.Expr);
            return null;
        }

        public object VisitFunctionStmt(Stmt.Function stmt)
        {
            Declare(stmt.Name);
            Define(stmt.Name);

            ResolveFunction(stmt, FunctionType.FUNCTION);
            return null;
        }

        public object VisitIfStmt(Stmt.If stmt)
        {
            Resolve(stmt.Condition);
            Resolve(stmt.ThenBranch);
            if (stmt.ElseBranch != null) { Resolve(stmt.ElseBranch); }
            return null;
        }

        public object VisitPrintStmt(Stmt.Print stmt)
        {
            Resolve(stmt.Expr);
            return null;
        }

        public object VisitReturnStmt(Stmt.Return stmt)
        {
            if (currentFunction == FunctionType.NONE)
            {
                Lox.Error(stmt.Keyword, "Can't return from top-level code.");
            }

            if (stmt.Value != null)
            {
                if (currentFunction == FunctionType.INITIALIZER)
                {
                    Lox.Error(stmt.Keyword, "Can't return a value from an initializer.");
                }

                Resolve(stmt.Value);
            }

            return null;
        }

        public object VisitVarStmt(Stmt.Var stmt)
        {
            Declare(stmt.Name);
            if (stmt.Initializer != null)
            {
                Resolve(stmt.Initializer);
            }
            Define(stmt.Name);

            return null;
        }

        public object VisitWhileStmt(Stmt.While stmt)
        {
            Resolve(stmt.Condition);
            Resolve(stmt.Body);
            return null;
        }

        public object VisitAssignExpr(Expr.Assign expr)
        {
            Resolve(expr.Value);
            ResolveLocal(expr, expr.Name);
            return null;
        }

        public object VisitBinaryExpr(Expr.Binary expr)
        {
            Resolve(expr.Left);
            Resolve(expr.Right);
            return null;
        }

        public object VisitCallExpr(Expr.Call expr)
        {
            Resolve(expr.Callee);

            foreach (var argument in expr.Arguments)
            {
                Resolve(argument);
            }

            return null;
        }

        public object VisitGetExpr(Expr.Get expr)
        {
            Resolve(expr.Object);
            return null;
        }

        public object VisitGroupingExpr(Expr.Grouping expr)
        {
            Resolve(expr.Expression);
            return null;
        }

        public object VisitLiteralExpr(Expr.Literal expr)
        {
            return null;
        }

        public object VisitLogicalExpr(Expr.Logical expr)
        {
            Resolve(expr.Left);
            Resolve(expr.Right);
            return null;
        }

        public object VisitSetExpr(Expr.Set expr)
        {
            Resolve(expr.Value);
            Resolve(expr.Object);
            return null;
        }

        public object VisitSuperExpr(Expr.Super expr)
        {
            if (currentClass == ClassType.NONE)
            {
                Lox.Error(expr.Keyword, "Can't use 'super' outside of a class.");
            }
            else if (currentClass != ClassType.SUBCLASS)
            {
                Lox.Error(expr.Keyword, "Can't use 'super' in a class with no superclass.");
            }

            ResolveLocal(expr, expr.Keyword);
            return null;
        }

        public object VisitThisExpr(Expr.This expr)
        {
            if (currentClass == ClassType.NONE)
            {
                Lox.Error(expr.Keyword, "Can't use 'this' outside of a clas.");
                return null;
            }

            ResolveLocal(expr, expr.Keyword);
            return null;
        }

        public object VisitUnaryExpr(Expr.Unary expr)
        {
            Resolve(expr.Right);
            return null;
        }

        public object VisitVariableExpr(Expr.Variable expr)
        {
            if (scopes.Any() && scopes.Peek().TryGetValue(expr.Name.Lexeme, out var variable) && variable == false)
            {
                Lox.Error(expr.Name, "Can't read local variable in its own initializer.");
            }

            ResolveLocal(expr, expr.Name);
            return null;
        }

        private void Resolve(Stmt stmt)
        {
            stmt.Accept(this);
        }

        private void Resolve(Expr expr)
        {
            expr.Accept(this);
        }

        private void ResolveFunction(Stmt.Function function, FunctionType type)
        {
            var enclosingFunction = currentFunction;
            currentFunction = type;

            BeginScope();
            foreach (var param in function.Params)
            {
                Declare(param);
                Define(param);
            }
            Resolve(function.Body);
            EndScope();
            currentFunction = enclosingFunction;
        }

        private void BeginScope()
        {
            scopes.Push(new Dictionary<string, bool>());
        }

        private void EndScope()
        {
            scopes.Pop();
        }

        private void Declare(Token name)
        {
            if (!scopes.Any()) { return; }

            var scope = scopes.Peek();
            if (scope.ContainsKey(name.Lexeme))
            {
                Lox.Error(name, "Already variable with this name in this scope.");
            }
            scope[name.Lexeme] = false;
        }

        private void Define(Token name)
        {
            if (!scopes.Any()) { return; }
            scopes.Peek()[name.Lexeme] = true;
        }

        private void ResolveLocal(Expr expr, Token name)
        {
            for (var i = 0; i < scopes.Count; i++)
            {
                if (scopes.ElementAt(i).ContainsKey(name.Lexeme))
                {
                    interpreter.Resolve(expr, i);
                    return;
                }
            }
        }
    }
}
