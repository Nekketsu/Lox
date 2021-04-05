using System.Collections.Generic;

namespace CraftingInterpreters.Lox
{
    public class Environment
    {
        public Environment Enclosing { get; }
        public Dictionary<string, object> Values { get; } = new Dictionary<string, object>();

        public Environment()
        {
            Enclosing = null;
        }

        public Environment(Environment enclosing)
        {
            this.Enclosing = enclosing;
        }

        public object Get(Token name)
        {
            if (Values.TryGetValue(name.Lexeme, out var value))
            {
                return value;
            }

            if (Enclosing != null) { return Enclosing.Get(name); }

            throw new RuntimeError(name, $"Undefined variable '{name.Lexeme}'.");
        }

        public void Assign(Token name, object value)
        {
            if (Values.ContainsKey(name.Lexeme))
            {
                Values[name.Lexeme] = value;
                return;
            }

            if (Enclosing != null)
            {
                Enclosing.Assign(name, value);
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
                environment = environment.Enclosing;
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
