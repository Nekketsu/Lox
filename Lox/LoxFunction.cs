namespace CraftingInterpreters.Lox
{
    public class LoxFunction : LoxCallable
    {
        private readonly Stmt.Function declaration;
        private readonly Environment closure;

        private readonly bool isInitializer;

        public LoxFunction(Stmt.Function declaration, Environment closure, bool isInitializer)
        {
            this.isInitializer = isInitializer;
            this.closure = closure;
            this.declaration = declaration;
        }

        public LoxFunction Bind(LoxInstance instance)
        {
            var environment = new Environment(closure);
            environment.Define("this", instance);
            return new LoxFunction(declaration, environment, isInitializer);
        }

        public object Call(Interpreter interpreter, object[] arguments)
        {
            var environment = new Environment(closure);
            for (var i = 0; i < declaration.Params.Length; i++)
            {
                environment.Define(declaration.Params[i].Lexeme, arguments[i]);
            }

            try
            {
                interpreter.ExecuteBlock(declaration.Body, environment);
            }
            catch (Return returnValue)
            {
                if (isInitializer) { return closure.GetAt(0, "this"); }

                return returnValue.Value;
            }

            if (isInitializer) { return closure.GetAt(0, "this"); }
            return null;
        }

        public int Arity => declaration.Params.Length;

        public override string ToString()
        {
            return $"<fn {declaration.Name.Lexeme}>";
        }
    }
}
