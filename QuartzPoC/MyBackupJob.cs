using Quartz;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace QuartzPooc
{
    class MyBackupJob : IJob
    {
        private readonly MyTask _task;
        private readonly IMySevice _service;

        public MyBackupJob(IMySevice sevice)
        {

            _service = sevice;
        }
        public Task Execute(IJobExecutionContext context)
        {
            int id = (int)context.JobDetail.JobDataMap["parameterId"];
            Console.WriteLine("Execute task" + _task); // task execution
            return _service.ExecuteBackupTask(id);
        }
    }
}
