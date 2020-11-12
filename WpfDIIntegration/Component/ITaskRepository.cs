using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WpfDIIntegration.Model;

namespace WpfDIIntegration.Component
{
    public interface ITaskRepository
    {
        List<RestoreTask> GetTasks();
    }
}
