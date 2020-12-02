using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
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
    public class RestoreDiskJob : IJob
    {
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IRestoreTaskDal _restoreTaskDal;
        private readonly IBackupService _backupService;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;
        private readonly ILogger _logger;

        public RestoreDiskJob(IBackupService backupService, ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IRestoreTaskDal restoreTaskDal, IStatusInfoDal statusInfoDal, IActivityLogDal activityLogDal, ILogger logger)
        {
            _backupService = backupService;
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _restoreTaskDal = restoreTaskDal;
            _statusInfoDal = statusInfoDal;
            _activityLogDal = activityLogDal;
            _logger = logger.ForContext<RestoreDiskJob>();
        }

        public Task Execute(IJobExecutionContext context)
        {
            bool result = false;
            bool availableResult = false;
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());
            var task = _taskInfoDal.Get(x => x.Id == taskId);
            _logger.Information("{@task} için restore disk görevi başlatıldı.", task);
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
                availableResult = _backupService.IsVolumeAvailable(task.StrObje[0]);

                if (availableResult)
                {
                    result = _backupService.RestoreBackupVolume(task);
                    var cleanChainResult = _backupService.CleanChain(task.StrObje[0]);
                    _logger.Information("{@task} {@value} zinciri temizlendi. Sonuç: {@cleanResult}", task, task.StrObje[0], cleanChainResult);
                }
                else
                    _logger.Information("{@task} {@value} volumu müsait değildi", task, task.StrObje[0]);

                // başarısızsa tekrar dene restore da başarısızsa tekrar dene yok
            }
            catch (Exception e)
            {
                _logger.Error(e, "{@Task} restore volume görevinde hata oluştu.", task);
                result = false;
                availableResult = false;
            }

            if (result || availableResult)
            {
                _logger.Verbose("{@task} volume job'ın result true ifindeyim", task);
                activityLog.Status = StatusType.Success;
                UpdateActivityAndTask(activityLog, task);
            }
            else
            {
                _logger.Verbose("{@task} volume job'ın result false ifindeyim", task);
                activityLog.Status = StatusType.Fail;
                UpdateActivityAndTask(activityLog, task);
            }

            _logger.Information("{@task} restore disk görevi bitirildi. Sonuç: {@result}.", task, result);
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
            taskInfo.Status = "Hazır"; // Resource eklenecek 
            _logger.Verbose("SchedulerId: {@schedulerId}.", taskInfo.ScheduleId);
            taskInfo.ScheduleId = taskInfo.ScheduleId.Split('*')[0];
            if (taskInfo.NextDate < DateTime.Now)
                taskInfo.ScheduleId = "";
            _logger.Verbose("Yeni SchedulerId: {@newscheduler}", taskInfo.ScheduleId);
            _taskInfoDal.Update(taskInfo);
            _backupService.RefreshIncDiffTaskFlag(true);
            _backupService.RefreshIncDiffLogFlag(true);
        }
    }
}
