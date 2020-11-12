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
    public class BackupFullJob : IJob
    {
        private readonly IBackupService _backupService;
        private readonly ITaskInfoDal _taskInfoRepository;
        private readonly IBackupStorageDal _backupStorageRepository;
        private readonly IStatusInfoDal _statusInfoRepository;

        public BackupFullJob(ITaskInfoDal taskInfoRepository, IBackupStorageDal backupStorageRepository, IStatusInfoDal statusInfoRepository, IBackupService backupService)
        {
            _backupService = backupService;
            _taskInfoRepository = taskInfoRepository;
            _backupStorageRepository = backupStorageRepository;
            _statusInfoRepository = statusInfoRepository;
        }

        public async Task Execute(IJobExecutionContext context)
        {
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());

            var task = _taskInfoRepository.Get(x => x.Id == taskId);
            task.BackupStorageInfo = _backupStorageRepository.Get(x => x.Id == task.BackupStorageInfoId);

            task.StatusInfo = _statusInfoRepository.Get(x => x.Id == task.StatusInfoId);

            JobExecutionException exception = null;
            bool result = true;

            try
            {
                result = _backupService.CreateFullBackup(task);
                // activity log burada basılacak
            }
            catch (Exception e)
            {
                if (task.BackupTaskInfo.FailTryAgain)
                {
                    exception = new JobExecutionException(e, context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
                }
            }

            if (!result)
            {
                exception = new JobExecutionException(context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
            }

            if (exception != null)
            {
                await Task.Delay(TimeSpan.FromMinutes(task.BackupTaskInfo.WaitNumberTryAgain));
                throw exception;
            }
        }
    }
}
