using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WpfDIIntegration.Model;

namespace WpfDIIntegration.Services
{
    class NewMyService : IMyService
    {
        public List<Person> GetPeople()
        {
            return new List<Person>
            {
                new Person { Name = "Ali", Age = 10},
                new Person { Name = "Veli", Age = 10}
            };
        }
    }
}
