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
    // BAŞTAN YAZILACAK
    public class BackupFullJob : IJob
    {
        private readonly IBackupService _backupService;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;

        public BackupFullJob(ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IStatusInfoDal statusInfoDal, IBackupService backupService, IActivityLogDal activityLogDal)
        {
            _backupService = backupService;
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _statusInfoDal = statusInfoDal;
            _activityLogDal = activityLogDal;
        }

        public async Task Execute(IJobExecutionContext context)
        {
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());

            var task = _taskInfoDal.Get(x => x.Id == taskId);
            task.LastWorkingDate = DateTime.Now; 
            task.Status = TaskStatusType.Working; // Resource eklenecek 
            _taskInfoDal.Update(task);

            task.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == task.BackupStorageInfoId);

            task.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);

            JobExecutionException exception = null;
            bool result = true;

            ActivityLog activityLog = new ActivityLog
            {
                TaskInfoName = task.Name,
                BackupStoragePath = task.BackupStorageInfo.Path,
                StartDate = DateTime.Now,
                Type = (DetailedMissionType)task.BackupTaskInfo.Type,
            };

            try
            {
                //Örneğin 2 tane 'c' görevi aynı anda service'e yollanamaz burada kontrol edilip gönderilmeli... Sonradan gelen görev başarısız sayılıp 
                //Refire kısmına gönderilmeli... ActivityLog'da bilgilendirilmeli
                //result = _backupService.CreateFullBackup(task);
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
                activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
                activityLog.StatusInfo.Status = StatusType.Fail;
                activityLog.StatusInfo.StrStatus = StatusType.Fail.ToString();
                var resultStatusInfo = _statusInfoDal.Add(activityLog.StatusInfo);
                activityLog.StatusInfoId = resultStatusInfo.Id;
                _activityLogDal.Add(activityLog);
                Console.WriteLine(exception.ToString());
                task.Status = TaskStatusType.Error; // Resource eklenecek 
                if (!context.JobDetail.Key.Name.Contains("Now"))
                {
                    task.NextDate = (context.Trigger.GetNextFireTimeUtc()).Value.LocalDateTime; // next date tekrar dene kısmında hatalı olmaması için test et / hatalıysa + fromMinutes
                    if (!task.BackupTaskInfo.FailTryAgain || (context.RefireCount > task.BackupTaskInfo.FailNumberTryAgain)) // now silme
                    {
                        var res = task.ScheduleId.Split('*');
                        task.ScheduleId = res[0];
                    }
                }
                _taskInfoDal.Update(task);
                await Task.Delay(TimeSpan.FromMinutes(task.BackupTaskInfo.WaitNumberTryAgain));
                throw exception;
            }

            activityLog.EndDate = DateTime.Now;
            activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
            activityLog.StatusInfo.Status = StatusType.Success;
            activityLog.StatusInfo.StrStatus = StatusType.Success.ToString();
            var resultStatusInfo2 = _statusInfoDal.Add(activityLog.StatusInfo);
            activityLog.StatusInfoId = resultStatusInfo2.Id;
            _activityLogDal.Add(activityLog);
            task.Status = TaskStatusType.Ready; // Resource eklenecek 
            Console.WriteLine(context.JobDetail.Key.Name);
            if (!context.JobDetail.Key.Name.Contains("Now"))
            {
                task.NextDate = (context.Trigger.GetNextFireTimeUtc()).Value.LocalDateTime;
                // now görevi sona erdi sil
                var res = task.ScheduleId.Split('*');
                task.ScheduleId = res[0];
            }
            _taskInfoDal.Update(task);
        }
    }
}
