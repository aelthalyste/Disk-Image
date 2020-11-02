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
    public class RestoreVolumeFactory : IJobFactory
    {
        private readonly IEntityRepository<BackupInfo> _backupInfoRepository;

        public RestoreVolumeFactory(IEntityRepository<BackupInfo> backupInfoRepository)
        {
            _backupInfoRepository = backupInfoRepository;
        }

        public IJob NewJob(TriggerFiredBundle bundle, IScheduler scheduler)
        {
            return new RestoreVolumeJob(_backupInfoRepository);
        }

        public void ReturnJob(IJob job)
        {
            throw new NotImplementedException();
        }
    }
}
