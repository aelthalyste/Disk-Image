using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WpfDIIntegration.Model;

namespace WpfDIIntegration.Services
{
    class MyService : IMyService
    {
        public List<Person> GetPeople()
        {
            return new List<Person>
            {
                new Person {Name = "Bob", Age = 27},
                new Person {Name = "Alice", Age = 28}
            };
        }

        public void ScheduleTask()
        {
            throw new NotImplementedException();
        }
    }
}
