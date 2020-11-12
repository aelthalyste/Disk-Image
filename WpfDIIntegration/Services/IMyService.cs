using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WpfDIIntegration.Model;

namespace WpfDIIntegration.Services
{
    public interface IMyService
    {
        List<Person> GetPeople();
        void ScheduleTask();
    }
}
