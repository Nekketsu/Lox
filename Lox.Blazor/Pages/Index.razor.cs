using Microsoft.AspNetCore.Components;
using System;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using Lox.Blazor.Services;
using System.Linq;

namespace Lox.Blazor.Pages
{
    public partial class Index
    {
        [Inject]
        public HttpClient HttpClient { get; set; }

        private StringBuilder loxOutput = new StringBuilder();

        public string Source { get; set; }
        public string[] Output { get; set; }
        public string[] Samples { get; set; }

        public Index()
        {
            Samples = new SamplesServices().Samples.Prepend(string.Empty).ToArray();
        }

        protected override void OnInitialized()
        {
            CraftingInterpreters.Lox.Lox.WriteLine = WriteLine;
            CraftingInterpreters.Lox.Lox.ErrorWriteLine = WriteLine;
        }

        private void Run()
        {
            if (Source != null)
            {
                CraftingInterpreters.Lox.Lox.Run(Source);

                var output = loxOutput.ToString();
                loxOutput.Clear();

                Output = output.Split(Environment.NewLine);
            }
        }

        private async Task SelectSample(ChangeEventArgs e)
        {
            var sample = e.Value.ToString();

            if (!string.IsNullOrWhiteSpace(sample))
            {
                Source = await HttpClient.GetStringAsync($"Samples/{sample}");
            }
        }

        private void WriteLine(string text)
        {
            loxOutput.AppendLine(text);
        }
    }
}
