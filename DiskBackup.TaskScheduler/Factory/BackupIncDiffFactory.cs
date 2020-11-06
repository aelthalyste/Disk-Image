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
    public class BackupIncDiffFactory : IJobFactory
    {
        private readonly IEntityRepository<TaskInfo> _taskInfoRepository;
        private readonly IEntityRepository<BackupStorageInfo> _backupStorageRepository;
        private readonly IEntityRepository<StatusInfo> _statusInfoRepository;

        public BackupIncDiffFactory (IEntityRepository<TaskInfo> taskInfoRepository, IEntityRepository<BackupStorageInfo> backupStorageRepository, IEntityRepository<StatusInfo> statusInfoRepository)
        {
            _taskInfoRepository = taskInfoRepository;
            _backupStorageRepository = backupStorageRepository;
            _statusInfoRepository = statusInfoRepository;
        }

        public IJob NewJob(TriggerFiredBundle bundle, IScheduler scheduler)
        {
            return new BackupIncDiffJob(_taskInfoRepository, _backupStorageRepository, _statusInfoRepository);
        }

        public void ReturnJob(IJob job)
        {
            throw new NotImplementedException();
        }
    }
}
