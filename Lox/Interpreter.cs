using System;
using System.Collections.Generic;
using static CraftingInterpreters.Lox.TokenType;

namespace CraftingInterpreters.Lox
{
    public class Interpreter : Expr.Visitor<object>, Stmt.Visitor<object>
    {
        public CraftingInterpreters.Lox.Environment Globals { get; } = new CraftingInterpreters.Lox.Environment();
        private CraftingInterpreters.Lox.Environment environment;

        public Interpreter()
        {
            environment = Globals;

            Globals.Define("Clock", new Clock());
        }

        public void Interpret(List<Stmt> statements)
        {
            try
            {
                foreach (var statement in statements)
                {
                    Execute(statement);
                }
            }
            catch (RuntimeError error)
            {
                Lox.RuntimeError(error);
            }
        }

        public object VisitLiteralExpr(Expr.Literal expr)
        {
            return expr.Value;
        }

        public object VisitLogicalExpr(Expr.Logical expr)
        {
            var left = Evaluate(expr.Left);

            if (expr.Operator.Type == TokenType.OR)
            {
                if (IsTruthy(left)) { return left; }
            }
            else
            {
                if (!IsTruthy(left)) { return false; }
            }

            return Evaluate(expr.Right);
        }

        public object VisitGroupingExpr(Expr.Grouping expr)
        {
            return Evaluate(expr.Expression);
        }

        public object VisitUnaryExpr(Expr.Unary expr)
        {
            var right = Evaluate(expr.Right);

            switch (expr.Operator.Type)
            {
                case BANG:
                    return !IsTruthy(right);
                case MINUS:
                    CheckNumberOperand(expr.Operator, right);
                    return -(double) right;
            }

            // Unreachable.
            return null;
        }

        public object VisitVariableExpr(Expr.Variable expr)
        {
            return environment.Get(expr.Name);
        }

        private void CheckNumberOperand(Token @operator, object operand)
        {
            if (operand is double) return;
            throw new RuntimeError(@operator, "Operand must be a number.");
        }
        
        private void CheckNumberOperands(Token @operator, object left, object right)
        {
            if (left is double && right is double) return;
            throw new RuntimeError(@operator, "Operands must be numbers.");
        }

        private bool IsTruthy(object @object)
        {
            if (@object == null) { return false; }
            if (@object is bool boolObject) { return boolObject; }
            return true;
        }

        private bool IsEqual(object a, object b)
        {
            if (a == null && b == null) { return true; }
            if (a == null) { return false; }

            return a.Equals(b);
        }

        private string Stringify(object @object)
        {
            if (@object == null) { return null; }

            if (@object is double objectDouble)
            {
                var text = @object.ToString();
                if (text.EndsWith(".0"))
                {
                    text = text.Substring(0, text.Length - 2);
                }
                return text;
            }

            return @object.ToString();
        }

        private object Evaluate(Expr expr)
        {
            return expr.Accept(this);
        }

        private void Execute(Stmt stmt)
        {
            stmt.Accept(this);
        }
        
        public void ExecuteBlock(Stmt[] statements, Environment environment)
        {
            var previous = this.environment;
            try
            {
                this.environment = environment;

                foreach (var statement in statements)
                {
                    Execute(statement);
                }
            }
            finally
            {
                this.environment = previous;
            }
        }

        public object VisitBlockStmt(Stmt.Block stmt)
        {
            ExecuteBlock(stmt.Statements, new CraftingInterpreters.Lox.Environment(environment));
            return null;
        }

        public object VisitExpressionStmt(Stmt.Expression stmt)
        {
            Evaluate(stmt.Expr);
            return null;
        }

        public object VisitFunctionStmt(Stmt.Function stmt)
        {
            var function = new LoxFunction(stmt, environment);
            environment.Define(stmt.Name.Lexeme, function);
            return null;
        }

        public object VisitIfStmt(Stmt.If stmt)
        {
            if (IsTruthy(Evaluate(stmt.Condition)))
            {
                Execute(stmt.ThenBranch);
            }
            else if (stmt.ElseBranch != null)
            {
                Execute(stmt.ElseBranch);
            }
            return null;
        }

        public object VisitPrintStmt(Stmt.Print stmt)
        {
            var value = Evaluate(stmt.Expr);
            Console.WriteLine(Stringify(value));
            return null;
        }

        public object VisitReturnStmt(Stmt.Return stmt)
        {
            object value = null;
            if (stmt.Value != null) { value = Evaluate(stmt.Value); }
            
            throw new Return(value);
        }

        public object VisitVarStmt(Stmt.Var stmt)
        {
            object value = null;
            if (stmt.Initializer != null)
            {
                value = Evaluate(stmt.Initializer);
            }

            environment.Define(stmt.Name.Lexeme, value);
            return null;
        }

        public object VisitWhileStmt(Stmt.While stmt)
        {
            while (IsTruthy(Evaluate(stmt.Condition)))
            {
                Execute(stmt.Body);
            }

            return null;
        }

        public object VisitAssignExpr(Expr.Assign expr)
        {
            var value = Evaluate(expr.Value);
            environment.Assign(expr.Name, value);
            return value;
        }
        
        public object VisitBinaryExpr(Expr.Binary expr)
        {
            var left = Evaluate(expr.Left);
            var right = Evaluate(expr.Right);

            switch (expr.Operator.Type)
            {
                case GREATER:
                    CheckNumberOperands(expr.Operator, left, right);
                    return (double)left > (double)right;
                case GREATER_EQUAL:
                    CheckNumberOperands(expr.Operator, left, right);
                    return (double)left >= (double)right;
                case LESS:
                    CheckNumberOperands(expr.Operator, left, right);
                    return (double)left < (double)right;
                case LESS_EQUAL:
                    CheckNumberOperands(expr.Operator, left, right);
                    return (double)left <= (double)right;
                case BANG_EQUAL:
                    return !IsEqual(left, right);
                case EQUAL_EQUAL:
                    return IsEqual(left, right);
                case MINUS:
                    CheckNumberOperands(expr.Operator, left, right);
                    return (double)left - (double)right;
                case PLUS:
                    if (left is double leftDouble && right is double rightDouble)
                    {
                        return leftDouble + rightDouble;
                    }

                    if (left is string leftString && right is string rightString)
                    {
                        return leftString + rightString;
                    }

                    throw new RuntimeError(expr.Operator, "Operands must be two numbers or two strings.");
                case SLASH:
                    CheckNumberOperands(expr.Operator, left, right);
                    return (double)left / (double)right;
                case STAR:
                    CheckNumberOperands(expr.Operator, left, right);
                    return (double)left * (double)right;
            }


            // Unreachable.
            return null;
        }

        public object VisitCallExpr(Expr.Call expr)
        {
            var callee = Evaluate(expr.Callee);

            var arguments = new List<object>();
            foreach (var argument in expr.Arguments)
            {
                arguments.Add(Evaluate(argument));
            }

            if (!(callee is LoxCallable function))
            {
                throw new RuntimeError(expr.Paren, "Can only call functions and classes.");
            }

            if (arguments.Count != function.Arity)
            {
                throw new RuntimeError(expr.Paren, $"Expected {function.Arity} arguments but got {arguments.Count}.");
            }

            return function.Call(this, arguments.ToArray());
        }
    }
}
