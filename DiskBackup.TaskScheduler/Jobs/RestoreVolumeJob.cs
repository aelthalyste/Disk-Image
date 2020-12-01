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
            _logger.Verbose("Restore Volume jobın en üstündeyim");
            bool result = false;
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());
            var task = _taskInfoDal.Get(x => x.Id == taskId);
            _logger.Information("{@task} için restore volume görevine başlandı.", task);
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
                result = _backupService.RestoreBackupVolume(task);
                var cleanChainResult = _backupService.CleanChain(task.StrObje[0]);
                _logger.Information("{@task} içinde {@value} zinciri temizlendi. Yeni zincir başlatılacak.", task, cleanChainResult);
                // başarısızsa tekrar dene restore da başarısızsa tekrar dene yok
            }
            catch (Exception e)
            {
                _logger.Error(e, "Restore volume görevinde hata oluştu. Task: {@Task}.", task);
                result = false;
            }

            if (result)
            {
                _logger.Verbose("Volume job'ın result true ifindeyim");
                activityLog.Status = StatusType.Success;
                UpdateActivityAndTask(activityLog, task);
            }
            else
            {
                _logger.Verbose("Volume job'ın result false ifindeyim");
                activityLog.Status = StatusType.Fail;
                UpdateActivityAndTask(activityLog, task);
            }
                        
            Console.WriteLine("Restore Job done");
            _logger.Information("{@task} için restore volume görevi bitirildi. Sonuç: {@result}.", task, result);           
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
            _logger.Verbose("Volume job'ın UpdateActivityAndTask() fonksiyonunda activity log eklendi.");
            taskInfo.Status = "Hazır"; // Resource eklenecek 
            _taskInfoDal.Update(taskInfo);
            _backupService.RefreshIncDiffTaskFlag(true);
            _backupService.RefreshIncDiffLogFlag(true);
        }
    }
}
