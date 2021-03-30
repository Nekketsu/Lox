namespace CraftingInterpreters.Lox
{
    public abstract class Expr
    {
        public interface Visitor<R>
        {
            R VisitAssignExpr(Assign expr);
            R VisitBinaryExpr(Binary expr);
            R VisitGroupingExpr(Grouping expr);
            R VisitLiteralExpr(Literal expr);
            R VisitLogicalExpr(Logical expr);
            R VisitUnaryExpr(Unary expr);
            R VisitVariableExpr(Variable expr);
        }

        public class Assign : Expr
        {
            public Token Name { get; }
            public Expr Value { get; }

            public Assign(Token name, Expr value)
            {
                Name = name;
                Value = value;
            }

            public override R Accept<R>(Visitor<R> visitor)
            {
                return visitor.VisitAssignExpr(this);
            }
        }

        public class Binary : Expr
        {
            public Expr Left { get; }
            public Token Operator { get; }
            public Expr Right { get; }

            public Binary(Expr left, Token @operator, Expr right)
            {
                Left = left;
                Operator = @operator;
                Right = right;
            }

            public override R Accept<R>(Visitor<R> visitor)
            {
                return visitor.VisitBinaryExpr(this);
            }
        }

        public class Grouping : Expr
        {
            public Expr Expression { get; }

            public Grouping(Expr expression)
            {
                Expression = expression;
            }

            public override R Accept<R>(Visitor<R> visitor)
            {
                return visitor.VisitGroupingExpr(this);
            }
        }

        public class Literal : Expr
        {
            public object Value { get; }

            public Literal(object value)
            {
                Value = value;
            }

            public override R Accept<R>(Visitor<R> visitor)
            {
                return visitor.VisitLiteralExpr(this);
            }
        }

        public class Logical : Expr
        {
            public Expr Left { get; }
            public Token Operator { get; }
            public Expr Right { get; }

            public Logical(Expr left, Token @operator, Expr right)
            {
                Left = left;
                Operator = @operator;
                Right = right;
            }

            public override R Accept<R>(Visitor<R> visitor)
            {
                return visitor.VisitLogicalExpr(this);
            }
        }

        public class Unary : Expr
        {
            public Token Operator { get; }
            public Expr Right { get; }

            public Unary(Token @operator, Expr right)
            {
                Operator = @operator;
                Right = right;
            }

            public override R Accept<R>(Visitor<R> visitor)
            {
                return visitor.VisitUnaryExpr(this);
            }
        }

        public class Variable : Expr
        {
            public Token Name { get; }

            public Variable(Token name)
            {
                Name = name;
            }

            public override R Accept<R>(Visitor<R> visitor)
            {
                return visitor.VisitVariableExpr(this);
            }
        }

        public abstract R Accept<R>(Visitor<R> visitor);
    }
}
