using System.Collections.Generic;
using System.Net.Http;
using System.Net.Http.Json;
using System.Threading.Tasks;

namespace Lox.Blazor.Services
{
    public class TestServices
    {
        public async Task<Dictionary<string, string[]>> GetTestsAsync(HttpClient httpClient, string uri)
        {
            var tests = await httpClient.GetFromJsonAsync<Dictionary<string, string[]>>("https://localhost:44397" + uri);

            return tests;
        }
    }
}
