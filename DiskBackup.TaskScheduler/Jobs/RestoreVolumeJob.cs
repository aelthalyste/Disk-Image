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
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler.Jobs
{
    public class RestoreVolumeJob : IJob
    {
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IRestoreTaskDal _restoreTaskDal;
        private readonly IBackupService _backupService;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;
        private readonly ILogger _logger;

        public RestoreVolumeJob(IBackupService backupService, ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IRestoreTaskDal restoreTaskDal, IStatusInfoDal statusInfoDal, IActivityLogDal activityLogDal, ILogger logger)
        {
            _backupService = backupService;
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _restoreTaskDal = restoreTaskDal;
            _statusInfoDal = statusInfoDal;
            _activityLogDal = activityLogDal;
            _logger = logger.ForContext<RestoreVolumeJob>();
        }

        public Task Execute(IJobExecutionContext context) // async ekleyeceğiz
        {
            byte result = 0;
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());
            var task = _taskInfoDal.Get(x => x.Id == taskId);
            _logger.Information("{@task} için restore volume görevi başlatıldı.", task);
            task.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == task.BackupStorageInfoId);
            task.RestoreTaskInfo = _restoreTaskDal.Get(x => x.Id == task.RestoreTaskId);

            ActivityLog activityLog = new ActivityLog
            {
                TaskInfoName = task.Name,
                BackupStoragePath = task.BackupStorageInfo.Path,
                StartDate = DateTime.Now,
                Type = DetailedMissionType.Restore
            };

            try
            {
                bool workingTask = false;
                var taskList = _taskInfoDal.GetList(x => x.Status != TaskStatusType.Ready && x.Status != TaskStatusType.FirstMissionExpected);
                foreach (var item in taskList)
                {
                    foreach (var itemObje in task.StrObje)
                    {
                        if (item.StrObje.Contains(itemObje))
                        {
                            // Okuma yapılan diskte işlem yapılamaz
                            workingTask = true;
                            Console.WriteLine("bu volumede çalışan bir görev var");
                            _logger.Information("{@task} için restore volume görevi çalıştırılamadı. {@letter} volumunde başka görev işliyor.", task, item.StrObje);
                        }
                    }
                }
                task.LastWorkingDate = DateTime.Now;

                if (!workingTask)
                {
                    task.Status = TaskStatusType.Working;
                    _taskInfoDal.Update(task);
                    _backupService.RefreshIncDiffTaskFlag(true);

                    result = _backupService.RestoreBackupVolume(task);
                    var cleanChainResult = _backupService.CleanChain(task.RestoreTaskInfo.TargetLetter[0]);
                    _logger.Information("{@task} {@value} zinciri temizlendi. Sonuç: {@cleanResult}", task, task.RestoreTaskInfo.TargetLetter[0], cleanChainResult);
                }

                // başarısızsa tekrar dene restore da başarısızsa tekrar dene yok
            }
            catch (Exception e)
            {
                _logger.Error(e, "{@Task} restore volume görevinde hata oluştu.", task);
                result = 0;
            }

            if (result == 1)
            {
                _logger.Verbose("{@task} volume job'ın result true ifindeyim", task);
                activityLog.Status = StatusType.Success;
                UpdateActivityAndTask(activityLog, task);
            }
            else if (result == 0)
            {
                _logger.Verbose("{@task} volume job'ın result false ifindeyim", task);
                activityLog.Status = StatusType.Fail;
                UpdateActivityAndTask(activityLog, task);
            }
            else if (result == 2) // bağlantı hatası
            {
                _logger.Verbose("{@task} volume job'ın result result bağlantı hatası ifindeyim", task);
                activityLog.Status = StatusType.ConnectionError;
                UpdateActivityAndTask(activityLog, task);
            }

            _logger.Information("{@task} restore volume görevi bitirildi. Sonuç: {@result}.", task, result);
            return Task.CompletedTask; // return değeri kaldırılacak ve async'e çevirilecek burası
        }

        private void UpdateActivityAndTask(ActivityLog activityLog, TaskInfo taskInfo)
        {
            activityLog.EndDate = DateTime.Now;
            activityLog.StrStatus = activityLog.Status.ToString();
            activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == taskInfo.StatusInfoId);
            var resultStatusInfo = _statusInfoDal.Add(activityLog.StatusInfo);
            activityLog.StatusInfoId = resultStatusInfo.Id;
            _activityLogDal.Add(activityLog);
            taskInfo.Status = TaskStatusType.Ready; // Resource eklenecek 
            _logger.Verbose("SchedulerId: {@schedulerId}.", taskInfo.ScheduleId);
            taskInfo.ScheduleId = taskInfo.ScheduleId.Split('*')[0];
            if (taskInfo.NextDate < DateTime.Now)
            {
                taskInfo.ScheduleId = "";
                taskInfo.NextDate = Convert.ToDateTime("01/01/0002");
            }
            _logger.Verbose("Yeni SchedulerId: {@newscheduler}", taskInfo.ScheduleId);
            _taskInfoDal.Update(taskInfo);
            _backupService.RefreshIncDiffTaskFlag(true);
            _backupService.RefreshIncDiffLogFlag(true);
        }
    }
}
