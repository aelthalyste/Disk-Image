using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using Quartz;
using Serilog;
using System;
using System.Collections.Generic;
using System.Linq;
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
        private readonly IBackupTaskDal _backupTaskDal;
        private readonly ILogger _logger;

        public BackupFullJob(ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IStatusInfoDal statusInfoDal, IBackupService backupService, IActivityLogDal activityLogDal, ILogger logger, IBackupTaskDal backupTaskDal)
        {
            _backupService = backupService;
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _statusInfoDal = statusInfoDal;
            _activityLogDal = activityLogDal;
            _backupTaskDal = backupTaskDal;
            _logger = logger.ForContext<BackupFullJob>();
        }

        public async Task Execute(IJobExecutionContext context)
        {
            int taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());

            TaskInfo task = _taskInfoDal.Get(x => x.Id == taskId);
            _logger.Information("{@task} için Full backup görevine başlandı.", task.Id + " " + task.Name);

            task.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == task.BackupStorageInfoId);
            task.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == task.BackupTaskId);
            task.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
            ResetStatusInfo(task);

            task.LastWorkingDate = DateTime.Now; 
            task.Status = TaskStatusType.Working; // Resource eklenecek 
            _taskInfoDal.Update(task);

            JobExecutionException exception = null;
            int result = 0;

            ActivityLog activityLog = new ActivityLog
            {
                TaskInfoName = task.Name,
                BackupStoragePath = task.BackupStorageInfo.Path,
                StartDate = DateTime.Now,
                Type = (DetailedMissionType)task.BackupTaskInfo.Type,
            };

            //Task listesi kontrol edilir ve çalışan, durdurulmuş ya da bozulmuş görevler tespit edilir. Bu görevler hali hazırda Disk'te veya Volume'da işlem yaptıkları için tekrar işleme alınmaz.
            List<TaskInfo> taskList = _taskInfoDal.GetList(x => x.Status != TaskStatusType.Ready && x.Status != TaskStatusType.FirstMissionExpected);
            foreach (TaskInfo item in taskList)
            {
                foreach (char itemObje in task.StrObje)
                {
                    if (item.StrObje.Contains(itemObje))
                    {
                        // Okuma yapılan diskte işlem yapılamaz
                        exception = new JobExecutionException();
                        _logger.Information("{@task} için Full backup görevi çalıştırılamadı. {@letter} volumunde başka görev işliyor.", task, item.StrObje);
                        if (item.Id == task.Id)
                        {
                            if (context.Trigger.GetNextFireTimeUtc() != null)
                                task.NextDate = (context.Trigger.GetNextFireTimeUtc()).Value.LocalDateTime;
                            _taskInfoDal.Update(task);
                            _backupService.RefreshIncDiffTaskFlag(true);
                            throw exception;
                        }
                    }
                }
            }

            try
            {
                if (DateTime.Now > task.NextDate)
                    task.LastWorkingDate = DateTime.Now;
                else
                    task.LastWorkingDate = task.NextDate;

                if (exception == null)
                {
                    task.Status = TaskStatusType.Working;
                    if (context.Trigger.GetNextFireTimeUtc() != null)
                    {
                        task.NextDate = context.Trigger.GetNextFireTimeUtc().Value.LocalDateTime;
                    }
                    _taskInfoDal.Update(task);
                    _backupService.RefreshIncDiffTaskFlag(true);
                    result = _backupService.CreateFullBackup(task);
                }
            }
            catch (Exception e)
            {
                _logger.Error(e, "Full backup görevinde hata oluştu. Task: {@Task}. Refirecount: {Count}", task, context.RefireCount);
                if (task.BackupTaskInfo.FailTryAgain)
                {
                    exception = new JobExecutionException(e, context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
                }
                else
                {
                    exception = new JobExecutionException();
                }
            }

            if (result == 0)
            {
                _logger.Information("{@task} için Full görevi bitirildi. Sonuç: NarDIWrapper'dan false geldi.", task.Name);
                if (task.BackupTaskInfo.FailTryAgain)
                {
                    exception = new JobExecutionException(context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
                }
                else
                {
                    exception = new JobExecutionException();
                }
            }

            if (exception != null) //herhangi bir exception oluştuysa başarısız olmuştur.
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
                    task.NextDate = context.Trigger.GetNextFireTimeUtc().Value.LocalDateTime; // next date tekrar dene kısmında hatalı olmaması için test et / hatalıysa + fromMinutes
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
                task.NextDate = context.Trigger.GetNextFireTimeUtc().Value.LocalDateTime;
                // now görevi sona erdi sil
                var res = task.ScheduleId.Split('*');
                task.ScheduleId = res[0];
            }
            _taskInfoDal.Update(task);
        }

        private void ResetStatusInfo(TaskInfo task)
        {
            task.StatusInfo.AverageDataRate = 0;
            task.StatusInfo.DataProcessed = 0;
            task.StatusInfo.FileName = "";
            task.StatusInfo.InstantDataRate = 0;
            task.StatusInfo.TimeElapsed = 0;
            task.StatusInfo.TotalDataProcessed = 100;
            _statusInfoDal.Update(task.StatusInfo);
        }
    }
}
