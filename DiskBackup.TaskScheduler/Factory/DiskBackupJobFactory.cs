using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Core;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler.Jobs;
using Quartz;
using Quartz.Spi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler.Factory
{
    public class DiskBackupJobFactory : IJobFactory
    {
        private readonly ILifetimeScope _scope;
        public DiskBackupJobFactory(ILifetimeScope scope)
        {
            _scope = scope;
        }
        public IJob NewJob(TriggerFiredBundle bundle, IScheduler scheduler)
        {
            return (IJob)_scope.Resolve(bundle.JobDetail.JobType);
        }

        public void ReturnJob(IJob job)
        {
            //throw new NotImplementedException();
        }
    }
}
