using System.Collections.Generic;
using static CraftingInterpreters.Lox.TokenType;

namespace CraftingInterpreters.Lox
{
    public class Interpreter : Expr.Visitor<object>, Stmt.Visitor<object>
    {
        private readonly Environment globals = new();
        private Environment environment;
        private readonly Dictionary<Expr, int> locals = new();

        public Interpreter()
        {
            environment = globals;

            globals.Define("clock", new Clock());
        }

        public void Interpret(Stmt[] statements)
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

        public object VisitSetExpr(Expr.Set expr)
        {
            var @object = Evaluate(expr.Object);

            if (@object is not LoxInstance loxInstance)
            {
                throw new RuntimeError(expr.Name, "Only instances have fields.");
            }

            var value = Evaluate(expr.Value);
            loxInstance.Set(expr.Name, value);
            return value;
        }

        public object VisitSuperExpr(Expr.Super expr)
        {
            var distance = locals[expr];
            var superclass = (LoxClass)environment.GetAt(distance, "super");

            var @object = (LoxInstance)environment.GetAt(distance - 1, "this");

            var method = superclass.FindMethod(expr.Method.Lexeme);

            if (method == null)
            {
                throw new RuntimeError(expr.Method, $"Undefined property '{expr.Method.Lexeme}'.");
            }

            return method.Bind(@object);
        }

        public object VisitThisExpr(Expr.This expr)
        {
            return LookUpVariable(expr.Keyword, expr);
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
                    return -(double)right;
            }

            // Unreachable.
            return null;
        }

        public object VisitVariableExpr(Expr.Variable expr)
        {
            return LookUpVariable(expr.Name, expr);
        }

        private object LookUpVariable(Token name, Expr expr)
        {
            if (locals.TryGetValue(expr, out var distance))
            {
                return environment.GetAt(distance, name.Lexeme);
            }
            else
            {
                return globals.Get(name);
            }
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
                var text = objectDouble.ToString(Lox.CultureInfo);
                if (text.EndsWith(".0"))
                {
                    text = text[0..^2];
                }
                return text;
            }
            else if (@object is bool objectBool)
            {
                return objectBool.ToString().ToLower();
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

        public void Resolve(Expr expr, int depth)
        {
            locals[expr] = depth;
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

        public object VisitClassStmt(Stmt.Class stmt)
        {
            object superclass = null;
            if (stmt.Superclass != null)
            {
                superclass = Evaluate(stmt.Superclass);
                if (!(superclass is LoxClass))
                {
                    throw new RuntimeError(stmt.Superclass.Name, "Superclass must be a class.");
                }
            }
            environment.Define(stmt.Name.Lexeme, null);

            if (stmt.Superclass != null)
            {
                environment = new Environment(environment);
                environment.Define("super", superclass);
            }

            var methods = new Dictionary<string, LoxFunction>();
            foreach (var method in stmt.Methods)
            {
                var function = new LoxFunction(method, environment, method.Name.Lexeme == "init");
                methods[method.Name.Lexeme] = function;
            }

            var @class = new LoxClass(stmt.Name.Lexeme, (LoxClass)superclass, methods);

            if (superclass != null)
            {
                environment = environment.Enclosing;
            }

            environment.Assign(stmt.Name, @class);
            return null;
        }

        public object VisitExpressionStmt(Stmt.Expression stmt)
        {
            Evaluate(stmt.Expr);
            return null;
        }

        public object VisitFunctionStmt(Stmt.Function stmt)
        {
            var function = new LoxFunction(stmt, environment, false);
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
            //Console.WriteLine(Stringify(value));
            Lox.WriteLine(Stringify(value));
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

            if (locals.TryGetValue(expr, out var distance))
            {
                environment.AssignAt(distance, expr.Name, value);
            }
            else
            {
                globals.Assign(expr.Name, value);
            }
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

            if (callee is not LoxCallable function)
            {
                throw new RuntimeError(expr.Paren, "Can only call functions and classes.");
            }

            if (arguments.Count != function.Arity)
            {
                throw new RuntimeError(expr.Paren, $"Expected {function.Arity} arguments but got {arguments.Count}.");
            }

            return function.Call(this, arguments.ToArray());
        }

        public object VisitGetExpr(Expr.Get expr)
        {
            var @object = Evaluate(expr.Object);
            if (@object is LoxInstance loxInstance)
            {
                return loxInstance.Get(expr.Name);
            }

            throw new RuntimeError(expr.Name, "Only instances have properties.");
        }
    }
}
