using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
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
        private readonly IBackupInfoDal _backupInfoRepository;

        public RestoreVolumeJob(IBackupInfoDal backupInfoRepository, IBackupService backupService)
        {
            _backupInfoRepository = backupInfoRepository;
            _backupService = backupService;
        }

        public Task Execute(IJobExecutionContext context)
        {
            var volumeLetter = (char)context.JobDetail.JobDataMap["volumeLetter"];
            var backupInfoId = int.Parse(context.JobDetail.JobDataMap["backupInfoId"].ToString());

            var backupInfo = _backupInfoRepository.Get(x => x.Id == backupInfoId);

            var result = _backupService.RestoreBackupVolume(backupInfo, volumeLetter);

            // başarısızsa tekrar dene
            // activity log burada basılacak

            return Task.CompletedTask;
        }
    }
}
