using System.Collections.Generic;

namespace CraftingInterpreters.Lox
{
    public class Environment
    {
        private readonly Environment enclosing;
        public Dictionary<string, object> Values { get; } = new Dictionary<string, object>();

        public Environment()
        {
            enclosing = null;
        }

        public Environment(Environment enclosing)
        {
            this.enclosing = enclosing;
        }

        public object Get(Token name)
        {
            if (Values.TryGetValue(name.Lexeme, out var value))
            {
                return value;
            }

            if (enclosing != null) { return enclosing.Get(name); }

            throw new RuntimeError(name, $"Undefined variable '{name.Lexeme}'.");
        }

        public void Assign(Token name, object value)
        {
            if (Values.ContainsKey(name.Lexeme))
            {
                Values[name.Lexeme] = value;
                return;
            }

            if (enclosing != null)
            {
                enclosing.Assign(name, value);
                return;
            }

            throw new RuntimeError(name, $"Undefined variable '{name.Lexeme}.'");
        }

        public void Define(string name, object value)
        {
            Values[name] = value;
        }

        private Environment Ancestor(int distance)
        {
            var environment = this;
            for (var i = 0; i < distance; i++)
            {
                environment = environment.enclosing;
            }

            return environment;
        }

        public object GetAt(int distance, string name)
        {
            return Ancestor(distance).Values[name];
        }

        public void AssignAt(int distance, Token name, object value)
        {
            Ancestor(distance).Values[name.Lexeme] = value;
        }
    }
}
