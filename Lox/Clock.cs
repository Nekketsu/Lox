namespace CraftingInterpreters.Lox
{
    public class Clock : LoxCallable
    {
        public int Arity => 0;

        public object Call(Interpreter interpreter, object[] arguments)
        {
            return (double)System.Environment.TickCount;
        }

        public override string ToString() => "<native fn>";
    }
}
