using System;
using System.Text;

namespace CraftingInterpreters.Lox
{
    public class AstPrinter : Expr.Visitor<string>
    {
        public string Print(Expr expr)
        {
            return expr.Accept(this);
        }

        public string VisitBinaryExpr(Expr.Binary expr)
        {
            return Parenthesize(expr.Operator.Lexeme, expr.Left, expr.Right);
        }

        public string VisitGroupingExpr(Expr.Grouping expr)
        {
            return Parenthesize("group", expr.Expression);
        }

        public string VisitLiteralExpr(Expr.Literal expr)
        {
            if (expr.Value == null) { return "nil"; }
            return expr.Value.ToString();
        }

        public string VisitUnaryExpr(Expr.Unary expr)
        {
            return Parenthesize(expr.Operator.Lexeme, expr.Right);
        }

        private string Parenthesize(string name, params Expr[] exprs)
        {
            var builder = new StringBuilder();

            builder.Append("(").Append(name);
            foreach (var expr in exprs)
            {
                builder.Append(" ");
                builder.Append(expr.Accept(this));
            }
            builder.Append(")");

            return builder.ToString();
        }

        // public static void Main(string[] args)
        // {
        //     var expression = new Expr.Binary(
        //             new Expr.Unary(
        //                 new Token(TokenType.MINUS, "-", null, 1),
        //                 new Expr.Literal(123)),
        //             new Token(TokenType.STAR, "*", null, 1),
        //             new Expr.Grouping(
        //                 new Expr.Literal(45.67)));

        //     Console.WriteLine(new AstPrinter().Print(expression));
        // }
    }
}
