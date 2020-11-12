using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WpfDIIntegration.Model;

namespace WpfDIIntegration.Component
{
    class TaskRepository : ITaskRepository
    {
        public List<RestoreTask> GetTasks()
        {
            return new List<RestoreTask>
            {
                new RestoreTask{Id=1,TaskName="task 1"},
                new RestoreTask{Id=2,TaskName="task 2"}
            };
        }
    }
}
