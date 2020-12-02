using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Core;
using DiskBackup.Entities.Concrete;
using Quartz;
using Serilog;
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
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;
        private readonly IBackupTaskDal _backupTaskDal;
        private readonly ILogger _logger;

        public BackupIncDiffJob(ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IStatusInfoDal statusInfoDal, IBackupService backupService, IActivityLogDal activityLogDal, IBackupTaskDal backupTaskDal, ILogger logger)
        {
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _statusInfoDal = statusInfoDal;
            _backupService = backupService;
            _activityLogDal = activityLogDal;
            _backupTaskDal = backupTaskDal;
            _logger = logger.ForContext<BackupIncDiffJob>();
        }

        public async Task Execute(IJobExecutionContext context)
        {
            Console.WriteLine("Job'a başlandı");
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());

            var task = _taskInfoDal.Get(x => x.Id == taskId);
            _logger.Information("{@task} için Incremental-Differantial görevine başlandı.", task.Id + " " + task.Name);

            task.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == task.BackupStorageInfoId);
            task.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
            task.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == task.BackupTaskId);

            JobExecutionException exception = null;
            byte result = 1;

            ActivityLog activityLog = new ActivityLog
            {
                TaskInfoName = task.Name,
                BackupStoragePath = task.BackupStorageInfo.Path,
                StartDate = DateTime.Now,
                Type = (DetailedMissionType)task.BackupTaskInfo.Type,
            };

            try
            {
                var taskList = _taskInfoDal.GetList(x => x.Status != "Hazır" && x.Status != "İlk Görev Bekleniyor");
                foreach (var item in taskList)
                {
                    foreach (var itemObje in task.StrObje)
                    {
                        if (item.StrObje.Contains(itemObje))
                        {
                            // Okuma yapılan diskte işlem yapılamaz
                            exception = new JobExecutionException();
                            Console.WriteLine("bu volumede çalışan bir görev var");
                        }
                    }
                }
                task.LastWorkingDate = DateTime.Now;

                if (exception == null)
                {
                    task.Status = "Çalışıyor"; // Resource eklenecek 
                    if (context.Trigger.GetNextFireTimeUtc() != null)
                    {
                        task.NextDate = (context.Trigger.GetNextFireTimeUtc()).Value.LocalDateTime;
                    }
                    _taskInfoDal.Update(task);
                    _backupService.RefreshIncDiffTaskFlag(true);
                    result = _backupService.CreateIncDiffBackup(task);

                    //for (int i = 0; i < 100000; i++)
                    //{
                    //    Console.WriteLine(i);
                    //}
                }


                Console.WriteLine("Done");
            }
            catch (Exception e)
            {
                _logger.Error(e, "Incremental-Differantial backup görevinde hata oluştu. Task: {@Task}. Refirecount: {Count}", task, context.RefireCount);
                Console.WriteLine("Catch'e düştü");
                if (task.BackupTaskInfo.FailTryAgain)
                {
                    exception = new JobExecutionException(e, context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
                }
                else
                {
                    Console.WriteLine(e.Message);
                    exception = new JobExecutionException();
                }
            }

            if (result == 0)
            {
                Console.WriteLine("Batuhan'dan false değer geldi");
                _logger.Information("{@task} için Incremental-Differantial görevi bitirildi. Sonuç: NarDIWrapper'dan false geldi.", task.Id + " " + task.Name);
                if (task.BackupTaskInfo.FailTryAgain)
                {
                    exception = new JobExecutionException(context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
                }
                else
                {
                    exception = new JobExecutionException();
                }
            }

            if (exception != null) // başarısız
            {
                //now görevi sona erdi sil
                activityLog.Status = StatusType.Fail;
                UpdateActivityAndTask(activityLog, task);
                await Task.Delay(TimeSpan.FromMinutes(task.BackupTaskInfo.WaitNumberTryAgain));
                throw exception;
            }

            if (result == 1) // başarılı
            {
                _logger.Information("{@task} için Incremental-Differantial görevi bitirildi. Sonuç: Başarılı.", task.Id + " " + task.Name);
                activityLog.Status = StatusType.Success;
                UpdateActivityAndTask(activityLog, task);
            }
            else if (result == 2) // durduruldu
            {
                _logger.Information("{@task} için Incremental-Differantial görevi durduruldu.", task.Id + " " + task.Name);
                activityLog.Status = StatusType.Cancel;
                UpdateActivityAndTask(activityLog, task);
            }

        }

        private void UpdateActivityAndTask(ActivityLog activityLog, TaskInfo taskInfo)
        {
            activityLog.EndDate = DateTime.Now;
            activityLog.StrStatus = activityLog.Status.ToString();
            activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == taskInfo.StatusInfoId);
            var resultStatusInfo = _statusInfoDal.Add(activityLog.StatusInfo);
            activityLog.StatusInfoId = resultStatusInfo.Id;
            _activityLogDal.Add(activityLog);
            taskInfo.Status = "Hazır"; // Resource eklenecek 
            _logger.Verbose("SchedulerId: {@schedulerId}.", taskInfo.ScheduleId);
            taskInfo.ScheduleId = taskInfo.ScheduleId.Split('*')[0];
            _logger.Verbose("Yeni SchedulerId: {@newscheduler}", taskInfo.ScheduleId);
            _taskInfoDal.Update(taskInfo);
            _backupService.RefreshIncDiffTaskFlag(true);
            _backupService.RefreshIncDiffLogFlag(true);
        }
    }
}
