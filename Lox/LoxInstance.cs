using System.Collections.Generic;

namespace CraftingInterpreters.Lox
{
    public class LoxInstance
    {
        private LoxClass @class;
        private readonly Dictionary<string, object> fields = new Dictionary<string, object>();

        public LoxInstance(LoxClass @class)
        {
            this.@class = @class;
        }

        public object Get(Token name)
        {
            if (fields.TryGetValue(name.Lexeme, out var value))
            {
                return value;
            }

            var method = @class.FindMethod(name.Lexeme);
            if (method != null) { return method.Bind(this); }

            throw new RuntimeError(name, $"Undefined property '{name.Lexeme}'.");
        }

        public void Set(Token name, object value)
        {
            fields[name.Lexeme] = value;
        }

        override public string ToString()
        {
            return $"{@class.Name} instance";
        }
    }
}
