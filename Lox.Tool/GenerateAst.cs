using System;
using System.IO;
using System.Linq;
using System.Reflection;

namespace Lox.Tool
{
    class GenerateAst
    {
        static int Main(string[] args)
        {
            if (args.Length != 1)
            {
                Console.Error.WriteLine($"Usage: {Assembly.GetExecutingAssembly().GetName().Name} <output_directory>");
                return 64;
            }
            var outputDir = args[0];

            DefineAst(outputDir, "Expr", new []
            {
                "Assign   : Token name, Expr value",
                "Binary   : Expr left, Token operator, Expr right",
                "Call     : Expr callee, Token paren, Expr[] arguments",
                "Grouping : Expr expression",
                "Literal  : object value",
                "Logical  : Expr left, Token operator, Expr right",
                "Unary    : Token operator, Expr right",
                "Variable : Token name"
            });

            DefineAst(outputDir, "Stmt", new []
            {
                "Block      : Stmt[] statements",
                "Expression : Expr expr",
                "Function   : Token name, Token[] params, Stmt[] body",
                "If         : Expr condition, Stmt thenBranch, Stmt elseBranch",
                "Print      : Expr expr",
                "Return     : Token keyword, Expr value",
                "Var        : Token name, Expr initializer",
                "While      : Expr condition, Stmt body"
            });

            return 0;
        }

        private static void DefineAst(string outputDir, string baseName, string[] types)
        {
            var path = Path.Combine(outputDir, $"{baseName}.cs");

            using (var stream = File.Create(path))
            {
                using (var writer = new StreamWriter(stream))
                {
                    writer.WriteLine("namespace CraftingInterpreters.Lox");
                    writer.WriteLine("{");
                    writer.WriteLine($"    public abstract class {baseName}");
                    writer.WriteLine("    {");

                    DefineVisitor(writer, baseName, types);
                    writer.WriteLine();

                    // The AST classes.
                    var lastType = types.Last();
                    foreach (var type in types)
                    {
                        var className = type.Split(':')[0].Trim();
                        var fields = type.Split(':')[1].Trim();
                        DefineType(writer, baseName, className, fields);
                        writer.WriteLine();
                    }

                    // The base accept() method
                    writer.WriteLine("        public abstract R Accept<R>(Visitor<R> visitor);");

                    writer.WriteLine("    }");
                    writer.WriteLine("}");
                }
            }
        }

        private static void DefineVisitor(StreamWriter writer, string baseName, string[] types)
        {
            writer.WriteLine("        public interface Visitor<R>");
            writer.WriteLine("        {");

            foreach (var type in types)
            {
                var typeName = type.Split(":")[0].Trim();
                writer.WriteLine($"            R Visit{typeName}{baseName}({typeName} {baseName.ToLower()});");
            }

            writer.WriteLine("        }");
        }

        private static void DefineType(StreamWriter writer, string baseName, string className, string fieldList)
        {
            writer.WriteLine($"        public class {className} : {baseName}");
            writer.WriteLine("        {");

            var fields = fieldList.Split(", ")
                                  .Select(f =>
                                  {
                                      var split = f.Split(' ');
                                      return new { Type = split[0], Field = GetFieldName(split[1]), Property = GetPropertyName(split[1]) };
                                  })
                                  .ToArray();
                                  
            // Fields.
            foreach (var field in fields)
            {
                writer.WriteLine($"            public {field.Type} {field.Property} {{ get; }}");
            }
            writer.WriteLine();

            // Constructor.
            writer.WriteLine($"            public {className}({string.Join(", ", fields.Select(f => $"{f.Type} {f.Field}"))})");
            writer.WriteLine("            {");

            // Store parameters in fields.
            foreach (var field in fields)
            {
                writer.WriteLine($"                {field.Property} = {field.Field};");
            }

            writer.WriteLine("            }");

            // Visitor pattern.
            writer.WriteLine();
            writer.WriteLine("            public override R Accept<R>(Visitor<R> visitor)");
            writer.WriteLine("            {");
            writer.WriteLine($"                return visitor.Visit{className}{baseName}(this);");
            writer.WriteLine("            }");

            writer.WriteLine("        }");
        }

        private static string GetFieldName(string name)
        {
            var keywords = new []
            {
                "operator",
                "params"
            };

            if (keywords.Contains(name))
            {
                return $"@{name}";
            }

            return name;
        }

        private static string GetPropertyName(string name)
        {
            var propertyName = name[0].ToString().ToUpper() + name.Substring(1);

            return propertyName;
        }
    }
}
