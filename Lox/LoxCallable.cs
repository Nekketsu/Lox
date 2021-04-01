namespace CraftingInterpreters.Lox
{
    public interface LoxCallable
    {
        public int Arity { get; }
        object Call(Interpreter interpreter, object[] arguments);
    }
}
