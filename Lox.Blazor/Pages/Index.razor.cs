using Lox.Blazor.Services;
using Microsoft.AspNetCore.Components;
using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace Lox.Blazor.Pages
{
    public partial class Index
    {
        [Inject]
        public HttpClient HttpClient { get; set; }

        private readonly StringBuilder loxOutput = new();

        public string Source { get; set; }
        public string[] Output { get; set; }

        public Dictionary<string, string[]> Tests { get; set; }
        public string SelectedFolder { get; set; }
        public string SelectedTest { get; set; }

        public bool ShouldClearSelectedTest { get; set; }

        private readonly TestServices testService;

        public Index()
        {
            testService = new TestServices();
        }

        protected override async Task OnInitializedAsync()
        {
            CraftingInterpreters.Lox.Lox.WriteLine = WriteLine;
            CraftingInterpreters.Lox.Lox.ErrorWriteLine = WriteLine;

            Tests = await testService.GetTestsAsync(HttpClient, "/Tests/Tests.json");
        }

        private void Run()
        {
            if (Source != null)
            {
                CraftingInterpreters.Lox.Lox.Reset();
                CraftingInterpreters.Lox.Lox.Run(Source);

                var output = loxOutput.ToString();
                loxOutput.Clear();

                Output = output.Split(Environment.NewLine);
            }
        }

        private void OnFolderSelected(ChangeEventArgs e)
        {
            SelectedFolder = e.Value?.ToString();
            SelectedTest = null;
        }

        private async Task OnTestSelected(ChangeEventArgs e)
        {
            SelectedTest = e.Value?.ToString();

            if (!string.IsNullOrWhiteSpace(SelectedTest))
            {
                Source = await HttpClient.GetStringAsync($"Tests/{SelectedFolder}/{SelectedTest}");
            }
        }

        private void WriteLine(string text)
        {
            loxOutput.AppendLine(text);
        }
    }
}
