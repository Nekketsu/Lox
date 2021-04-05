using System;
using System.Text;

namespace Lox.Blazor.Pages
{
    public partial class Index
    {
        private StringBuilder loxOutput = new StringBuilder();

        public string Source { get; set; }
        public string[] Output { get; set; }

        protected override void OnInitialized()
        {
            CraftingInterpreters.Lox.Lox.WriteLine = WriteLine;
            CraftingInterpreters.Lox.Lox.ErrorWriteLine = WriteLine;
        }

        private void Run()
        {
            CraftingInterpreters.Lox.Lox.Run(Source);

            var output = loxOutput.ToString();
            loxOutput.Clear();

            Output = output.Split(Environment.NewLine);
        }

        private void WriteLine(string text)
        {
            loxOutput.AppendLine(text);
        }
    }
}
