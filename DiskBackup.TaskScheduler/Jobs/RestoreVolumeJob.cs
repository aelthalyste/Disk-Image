using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Core;
using DiskBackup.Entities.Concrete;
using Quartz;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler.Jobs
{
    public class RestoreVolumeJob : IJob
    {
        private readonly IBackupService _backupService;
        private readonly IEntityRepository<BackupInfo> _backupInfoRepository;

        public RestoreVolumeJob(IEntityRepository<BackupInfo> backupInfoRepository)
        {
            _backupService = new BackupManager();
            _backupInfoRepository = backupInfoRepository;
        }

        public Task Execute(IJobExecutionContext context)
        {
            var volumeLetter = (char)context.JobDetail.JobDataMap["volumeLetter"];
            var backupInfoId = (int)context.JobDetail.JobDataMap["backupInfoId"];

            var backupInfo = _backupInfoRepository.Get(x => x.Id == backupInfoId);

            var result = _backupService.RestoreBackupVolume(backupInfo, volumeLetter);

            // başarısızsa tekrar dene
            // activity log burada basılacak

            return Task.CompletedTask;
        }
    }
}
