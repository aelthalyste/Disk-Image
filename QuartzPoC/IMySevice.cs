using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace QuartzPooc
{
    interface IMySevice
    {
        Task ExecuteBackupTask(int taskId);
    }
}
