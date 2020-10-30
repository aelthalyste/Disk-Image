using Quartz;
using Quartz.Spi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace QuartzPooc
{
    class MyJobFactory : IJobFactory
    {
        private readonly ITaskRepository _repository;

        public MyJobFactory(ITaskRepository repository)
        {
            _repository = repository;
        }
        public IJob NewJob(TriggerFiredBundle bundle, IScheduler scheduler)
        {
            int id = (int)bundle.JobDetail.JobDataMap["parameterId"];
            return new MyBackupJob(_repository.GetById(id));
        }

        public void ReturnJob(IJob job)
        {
        }
    }
}
