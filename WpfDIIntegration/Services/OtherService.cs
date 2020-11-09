using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WpfDIIntegration.Services
{
    public class OtherService : IOtherService
    {
        public List<string> Cities()
        {
            return new List<string>
            {
                "Ankara",
                "Istanbul"
            };
        }
    }
}
