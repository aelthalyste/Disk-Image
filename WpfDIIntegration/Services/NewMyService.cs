using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WpfDIIntegration.Component;
using WpfDIIntegration.Model;

namespace WpfDIIntegration.Services
{
    class NewMyService : IMyService
    {
        private readonly Func<TaskManager> _createTaskManager;

        public NewMyService(Func<TaskManager> createTaskManager)
        {
            _createTaskManager = createTaskManager;
        }

        public List<Person> GetPeople()
        {
            return new List<Person>
            {
                new Person { Name = "Ali", Age = 10},
                new Person { Name = "Veli", Age = 10}
            };
        }

        public void ScheduleTask()
        {
            var taskManager = _createTaskManager();
            try
            {
                _ = taskManager.GetPersonName();
            }
            catch (Exception)
            {
            }
        }
    }
}
