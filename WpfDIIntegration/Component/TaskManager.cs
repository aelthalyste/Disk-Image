using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WpfDIIntegration.Model;

namespace WpfDIIntegration.Component
{
    public class TaskManager
    {
        private readonly ITaskRepository _taskRepository;
        private readonly Person _person;

        public TaskManager(ITaskRepository taskRepository)
        {
            _taskRepository = taskRepository;
        }
        public TaskManager(ITaskRepository taskRepository, Person person)
        {
            _taskRepository = taskRepository;
            _person = person;
        }

        public string GetPersonName()
        {
            _ = _taskRepository.GetTasks();
            return _person.Name;
        }
    }
}
