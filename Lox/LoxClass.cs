using System.Collections.Generic;

namespace CraftingInterpreters.Lox
{
    public class LoxClass : LoxCallable
    {
        public string Name { get; }
        private readonly Dictionary<string, LoxFunction> methods;

        public LoxClass(string name, Dictionary<string, LoxFunction> methods)
        {
            Name = name;
            this.methods = methods;
        }

        public LoxFunction FindMethod(string name)
        {
            if (methods.TryGetValue(name, out var method))
            {
                return method;
            }

            return null;
        }

        public override string ToString()
        {
            return Name;
        }

        public object Call(Interpreter interpreter, object[] arguments)
        {
            var instance = new LoxInstance(this);
            var initializer = FindMethod("init");
            if (initializer != null)
            {
                initializer.Bind(instance).Call(interpreter, arguments);
            }

            return instance;
        }

        public int Arity
        {
            get
            {
                var initializer = FindMethod("init");
                if (initializer == null) { return 0; }
                return initializer.Arity;
            }
        }
    }
}
