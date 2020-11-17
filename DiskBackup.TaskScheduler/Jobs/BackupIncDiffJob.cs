﻿using DiskBackup.Business.Abstract;
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
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;
        private readonly IBackupTaskDal _backupTaskDal;

        public static bool _refreshIncDiffTaskFlag { get; set; } = false;
        public static bool _refreshIncDiffLogFlag { get; set; } = false;

        public BackupIncDiffJob(ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IStatusInfoDal statusInfoDal, IBackupService backupService, IActivityLogDal activityLogDal, IBackupTaskDal backupTaskDal)
        {
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _statusInfoDal = statusInfoDal;
            _backupService = backupService;
            _activityLogDal = activityLogDal;
            _backupTaskDal = backupTaskDal;
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
                var taskList = _taskInfoDal.GetList(x => x.Status == "Çalışıyor");
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
                task.Status = "Çalışıyor"; // Resource eklenecek 
                _taskInfoDal.Update(task);
                _refreshIncDiffTaskFlag = true;
                if (exception == null)
                    result = _backupService.CreateIncDiffBackup(task);

                //for (int i = 0; i < 10000; i++)
                //{
                //    Console.WriteLine(i);
                //}
                Console.WriteLine("Done");
            }
            catch (Exception e)
            {
                Console.WriteLine("Catch'e düştü");
                if (task.BackupTaskInfo.FailTryAgain)
                {
                    exception = new JobExecutionException(e, context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
                }
            }

            if (!result)
            {
                Console.WriteLine("Batuhan'dan false değer geldi");
                if (task.BackupTaskInfo.FailTryAgain)
                {
                    exception = new JobExecutionException(context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
                }
            }

            if (exception != null)
            {
                //now görevi sona erdi sil
                activityLog.EndDate = DateTime.Now;
                activityLog.Status = StatusType.Fail;
                activityLog.StrStatus = StatusType.Fail.ToString();
                activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
                var resultStatusInfo = _statusInfoDal.Add(activityLog.StatusInfo);
                activityLog.StatusInfoId = resultStatusInfo.Id;
                _activityLogDal.Add(activityLog);
                Console.WriteLine(exception.ToString());
                //task.Status = "Hata"; // Resource eklenecek 
                if (context.Trigger.GetNextFireTimeUtc() != null)
                {
                    task.NextDate = (context.Trigger.GetNextFireTimeUtc()).Value.LocalDateTime;
                }
                _taskInfoDal.Update(task);
                _refreshIncDiffLogFlag = true;
                await Task.Delay(TimeSpan.FromMinutes(task.BackupTaskInfo.WaitNumberTryAgain));
                throw exception;
            }

            activityLog.EndDate = DateTime.Now;
            activityLog.Status = StatusType.Success;
            activityLog.StrStatus = StatusType.Success.ToString();
            activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
            var resultStatusInfo2 = _statusInfoDal.Add(activityLog.StatusInfo);
            activityLog.StatusInfoId = resultStatusInfo2.Id;
            _activityLogDal.Add(activityLog);
            task.Status = "Hazır"; // Resource eklenecek 
            Console.WriteLine(context.JobDetail.Key.Name);
            if (context.Trigger.GetNextFireTimeUtc() != null)
            {
                task.NextDate = (context.Trigger.GetNextFireTimeUtc()).Value.LocalDateTime;
            }
            _taskInfoDal.Update(task);
            _refreshIncDiffTaskFlag = true;
            _refreshIncDiffLogFlag = true;
        }
    }
}
