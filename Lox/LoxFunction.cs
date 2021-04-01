namespace CraftingInterpreters.Lox
{
    public class LoxFunction : LoxCallable
    {
        private readonly Stmt.Function declaration;
        private readonly Environment closure;

        public LoxFunction(Stmt.Function declaration, Environment closure)
        {
            this.closure = closure;
            this.declaration = declaration;
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
                return returnValue.Value;
            }
            return null;
        }

        public int Arity => declaration.Params.Length;

        public override string ToString()
        {
            return $"<fn {declaration.Name.Lexeme}>";
        }
    }
}
