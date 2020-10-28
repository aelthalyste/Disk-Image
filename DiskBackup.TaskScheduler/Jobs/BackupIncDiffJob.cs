using DiskBackup.Business.Concrete;
using Quartz;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler.Jobs
{
    public class BackupIncDiffJob : IJob
    {
        public Task Execute(IJobExecutionContext context)
        {
            try
            {
                var result = TaskSchedulerManager._backupManager.CreateIncDiffBackup(TaskSchedulerManager._taskInfo, TaskSchedulerManager._backupStorageInfo);
            }
            catch (AggregateException ae)
            {
                // handle exception
            }
            return Task.CompletedTask;
        }
    }
}
