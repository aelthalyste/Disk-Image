﻿using DiskBackup.Business.Abstract;
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
        public static bool _refreshIncDiffTaskFlag { get; set; } = false;
        public static bool _refreshIncDiffLogFlag { get; set; } = false;

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
                var taskList = _taskInfoDal.GetList(x => x.Status != "Hazır");
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
                    _refreshIncDiffTaskFlag = true;
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
                _logger.Error(e, "Incremental backup görevinde hata oluştu. Task: {@Task}. Refirecount: {Count}", task, context.RefireCount);
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
                activityLog.Status = StatusType.Success;
                UpdateActivityAndTask(activityLog, task);
            }
            else if (result == 2) // durduruldu
            {
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
            _taskInfoDal.Update(taskInfo);
            _refreshIncDiffTaskFlag = true;
            _refreshIncDiffLogFlag = true;
        }
    }
}
