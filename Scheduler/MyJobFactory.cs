using Quartz;
using Quartz.Spi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Scheduler
{
    public class MyJobFactory : IJobFactory
    {
        public IJob NewJob(TriggerFiredBundle bundle, IScheduler scheduler)
        {
            throw new NotImplementedException();
        }

        public void ReturnJob(IJob job)
        {
            throw new NotImplementedException();
        }
    }
}
