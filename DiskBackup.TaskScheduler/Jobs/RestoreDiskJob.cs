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
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());
            var task = _taskInfoDal.Get(x => x.Id == taskId);
            _logger.Information("{@task} için restore disk görevine başlandı.", task);
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
                //result = _backupService.RestoreBackupDisk(task);
                // başarısızsa tekrar dene restore da başarısızsa tekrar dene yok
            }
            catch (Exception e)
            {
                _logger.Error(e, "Restore volume görevinde hata oluştu. Task: {@Task}.", task);
                result = false;
            }


            if (result)
            {
                activityLog.Status = StatusType.Success;
                UpdateActivityAndTask(activityLog, task);
            }
            else
            {
                activityLog.Status = StatusType.Fail;
                UpdateActivityAndTask(activityLog, task);
            }

            Console.WriteLine("Restore Job done");
            _logger.Information("{@task} için restore disk görevi bitirildi. Sonuç: {@result}.", task, result);
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
            _taskInfoDal.Update(taskInfo);
            _backupService.RefreshIncDiffTaskFlag(true);
            _backupService.RefreshIncDiffLogFlag(true);
        }
    }
}
