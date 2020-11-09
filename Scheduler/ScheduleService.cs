using Quartz.Spi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Scheduler
{
    public class ScheduleService
    {
        private readonly IJobFactory _jobFactory;

        public ScheduleService(IJobFactory jobFactory)
        {
            _jobFactory = jobFactory;
        }

        public void Test()
        {
            _ =_jobFactory.NewJob(null, null);
        }
    }
}
