using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Core;
using DiskBackup.Entities.Concrete;
using Quartz;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler.Jobs
{
    public class BackupIncDiffJob : IJob
    {
        private readonly IBackupService _backupService;
        private readonly ITaskInfoDal _taskInfoRepository;
        private readonly IBackupStorageDal _backupStorageRepository;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;
        private readonly IBackupTaskDal _backupTaskDal;

        public BackupIncDiffJob(ITaskInfoDal taskInfoRepository, IBackupStorageDal backupStorageRepository, IStatusInfoDal statusInfoRepository, IBackupService backupService, IActivityLogDal activityLogDal, IBackupTaskDal backupTaskDal)
        {
            _taskInfoRepository = taskInfoRepository;
            _backupStorageRepository = backupStorageRepository;
            _statusInfoDal = statusInfoRepository;
            _backupService = backupService;
            _activityLogDal = activityLogDal;
            _backupTaskDal = backupTaskDal;
        }

        public async Task Execute(IJobExecutionContext context)
        {
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());

            var task = _taskInfoRepository.Get(x => x.Id == taskId);
            task.BackupStorageInfo = _backupStorageRepository.Get(x => x.Id == task.BackupStorageInfoId);

            task.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
            task.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == task.BackupTaskId);

            JobExecutionException exception = null;
            bool result = true;

            ActivityLog activityLog = new ActivityLog {
                TaskInfoName = task.Name,
                BackupStorageInfo = task.BackupStorageInfo,
                BackupStorageInfoId = task.BackupStorageInfoId,
                StartDate = DateTime.Now,
                Type = (DetailedMissionType)task.BackupTaskInfo.Type,
                
            };

            try
            {
                //Örneğin 2 tane 'c' görevi aynı anda service'e yollanamaz burada kontrol edilip gönderilmeli... Sonradan gelen görev başarısız sayılıp 
                //Refire kısmına gönderilmeli... ActivityLog'da bilgilendirilmeli
                result = _backupService.CreateIncDiffBackup(task);
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
                activityLog.EndDate = DateTime.Now;
                activityLog.Status = StatusType.Fail;
                activityLog.StrStatus = StatusType.Fail.ToString();
                activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
                var resultStatusInfo = _statusInfoDal.Add(activityLog.StatusInfo);
                activityLog.StatusInfoId = resultStatusInfo.Id;

                await Task.Delay(TimeSpan.FromMinutes(task.BackupTaskInfo.WaitNumberTryAgain));
                throw exception;
            }

            activityLog.EndDate = DateTime.Now;
            activityLog.Status = StatusType.Success;
            activityLog.StrStatus = StatusType.Success.ToString();
            activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
            var resultStatusInfo2 = _statusInfoDal.Add(activityLog.StatusInfo);
            activityLog.StatusInfoId = resultStatusInfo2.Id;
        }
    }
}
