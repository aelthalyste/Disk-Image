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
    public class BackupFullFactory : IJobFactory
    {
        private readonly IEntityRepository<TaskInfo> _taskInfoRepository;
        private readonly IEntityRepository<BackupStorageInfo> _backupStorageRepository;

        public BackupFullFactory(IEntityRepository<TaskInfo> taskInfoRepository, IEntityRepository<BackupStorageInfo> backupStorageRepository)
        {
            _taskInfoRepository = taskInfoRepository;
            _backupStorageRepository = backupStorageRepository;
        }

        public IJob NewJob(TriggerFiredBundle bundle, IScheduler scheduler)
        {
            return new BackupFullJob(_taskInfoRepository, _backupStorageRepository);
        }

        public void ReturnJob(IJob job)
        {
            throw new NotImplementedException();
        }
    }
}
