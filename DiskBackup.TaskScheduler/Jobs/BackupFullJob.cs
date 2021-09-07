using DiskBackup.Business.Abstract;
using DiskBackup.Communication;
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
        private IEMailOperations _emailOperations;
        private readonly ILogger _logger;

        public BackupFullJob(ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IStatusInfoDal statusInfoDal, IBackupService backupService, IActivityLogDal activityLogDal, ILogger logger, IBackupTaskDal backupTaskDal, IEMailOperations emailOperations)
        {
            _backupService = backupService;
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _statusInfoDal = statusInfoDal;
            _activityLogDal = activityLogDal;
            _backupTaskDal = backupTaskDal;
            _emailOperations = emailOperations;
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
            //List<TaskInfo> taskList = _taskInfoDal.GetList(x => x.Status != TaskStatusType.Ready && x.Status != TaskStatusType.FirstMissionExpected);
            //foreach (TaskInfo item in taskList)
            //{
            //    foreach (char itemObje in task.StrObje)
            //    {
            //        if (item.StrObje.Contains(itemObje))
            //        {
            //            // Okuma yapılan diskte işlem yapılamaz
            //            exception = new JobExecutionException();
            //            _logger.Information("{@task} için Full backup görevi çalıştırılamadı. {@letter} volumunde başka görev işliyor.", task, item.StrObje);
            //            if (item.Id == task.Id)
            //            {
            //                if (context.Trigger.GetNextFireTimeUtc() != null)
            //                    task.NextDate = (context.Trigger.GetNextFireTimeUtc()).Value.LocalDateTime;
            //                _taskInfoDal.Update(task);
            //                _backupService.RefreshIncDiffTaskFlag(true);
            //                throw exception;
            //            }
            //        }
            //    }
            //}

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
                UpdateActivityAndTask(activityLog, task, StatusType.Fail);
                await Task.Delay(TimeSpan.FromMinutes(task.BackupTaskInfo.WaitNumberTryAgain));
                throw exception;
            }

            if (result == 1) // başarılı
            {
                _logger.Information("{@task} için Incremental-Differantial görevi bitirildi. Sonuç: Başarılı.", task);
                UpdateActivityAndTask(activityLog, task, StatusType.Success);
            }
            else if (result == 2) // durduruldu
            {
                _logger.Information("{@task} için Incremental-Differantial görevi durduruldu.", task);
                UpdateActivityAndTask(activityLog, task, StatusType.Cancel);
            }
            else if (result == 3)
            {
                _logger.Information("{@task} için Incremental-Differantial görevi yetersiz alandan dolayı başlatılamadı. Sonuç: Başarısız.", task);
                UpdateActivityAndTask(activityLog, task, StatusType.NotEnoughDiskSpace);
            }
            else if (result == 4)
            {
                _logger.Information("{@task} için Incremental-Differantial görevi NAS'a bağlanılamadığı için başlatılamadı. Sonuç: Başarısız.", task);
                UpdateActivityAndTask(activityLog, task, StatusType.ConnectionError);
            }
            else if (result == 5) // driver initialize edilemedi
            {
                _logger.Information("{@task} için Incremental-Differantial görevi driver initialize edilemediği için başlatılamadı. Sonuç: Başarısız.", task);
                UpdateActivityAndTask(activityLog, task, StatusType.DriverNotInitialized);
            }
            else if (result == 6) // backup alınacak path yok
            {
                _logger.Information("{@task} için Incremental-Differantial görevi aranan disk bulunamadığı için başlatılamadı. Sonuç: Başarısız.", task);
                UpdateActivityAndTask(activityLog, task, StatusType.PathNotFound);
            }
            else if (result == 8) // backup alınacak path yok
            {
                _logger.Information("{@task} için Incremental-Differantial görevi yeni zincir oluşturulamadığı için başlatılamadı. Sonuç: Başarısız.", task);
                UpdateActivityAndTask(activityLog, task, StatusType.NewChainNotStarted);
            }
        }

        private void UpdateActivityAndTask(ActivityLog activityLog, TaskInfo taskInfo, StatusType status)
        {
            taskInfo = _taskInfoDal.Get(x => x.Id == taskInfo.Id); // Aynı görev yeniden çalışmaya çalıştıysa next date değişmiş oluyor o zamanı aldığımızdan emin olmak için gerekli
            activityLog.EndDate = DateTime.Now;
            activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == taskInfo.StatusInfoId);
            activityLog.StatusInfo.Status = status;
            activityLog.StatusInfo.StrStatus = status.ToString();
            var resultTaskStatusInfo = _statusInfoDal.Update(activityLog.StatusInfo);
            var resultStatusInfo = _statusInfoDal.Add(activityLog.StatusInfo);
            activityLog.StatusInfoId = resultStatusInfo.Id;
            _activityLogDal.Add(activityLog);
            taskInfo.Status = TaskStatusType.Ready; // Resource eklenecek 
            _logger.Verbose("SchedulerId: {@schedulerId}.", taskInfo.ScheduleId);
            taskInfo.ScheduleId = taskInfo.ScheduleId.Split('*')[0];
            _logger.Verbose("Yeni SchedulerId: {@newscheduler}", taskInfo.ScheduleId);
            taskInfo.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == taskInfo.BackupTaskId);
            _taskInfoDal.Update(taskInfo);
            _backupService.RefreshIncDiffTaskFlag(true);
            _backupService.RefreshIncDiffLogFlag(true);

            _logger.Verbose("SendEmail çağırılıyor");
            try
            {
                taskInfo.StatusInfo = resultTaskStatusInfo;
                _emailOperations.SendTaskStatusEMail(taskInfo);
            }
            catch (Exception ex)
            {
                _logger.Error(ex + "Email gönderilemedi");
            }
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
