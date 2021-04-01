using System;

namespace CraftingInterpreters.Lox
{
    public class Return : Exception
    {
        public object Value { get; }

        public Return(object value)
        {
            Value = value;
        }
    }
}
