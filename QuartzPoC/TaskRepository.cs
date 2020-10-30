using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace QuartzPooc
{
    class TaskRepository : ITaskRepository
    {
        public MyTask GetById(int id)
        {
            return new MyTask();
        }
    }
}
