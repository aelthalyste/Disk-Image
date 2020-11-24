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
    public class RestoreVolumeJob : IJob
    {
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IRestoreTaskDal _restoreTaskDal;
        private readonly IBackupService _backupService;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;

        public RestoreVolumeJob(IBackupService backupService, ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IRestoreTaskDal restoreTaskDal, IStatusInfoDal statusInfoDal, IActivityLogDal activityLogDal)
        {
            _backupService = backupService;
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _restoreTaskDal = restoreTaskDal;
            _statusInfoDal = statusInfoDal;
            _activityLogDal = activityLogDal;
        }

        public Task Execute(IJobExecutionContext context) // async ekleyeceğiz
        {
            Console.WriteLine("Restore Job'a başlandı");
            var taskId = int.Parse(context.JobDetail.JobDataMap["taskId"].ToString());
            var task = _taskInfoDal.Get(x => x.Id == taskId);
            task.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == task.BackupStorageInfoId);
            task.RestoreTaskInfo = _restoreTaskDal.Get(x => x.Id == task.RestoreTaskId);

            ActivityLog activityLog = new ActivityLog
            {
                TaskInfoName = task.Name,
                BackupStoragePath = task.BackupStorageInfo.Path,
                StartDate = DateTime.Now,
                Type = DetailedMissionType.Restore
            };

            var result = _backupService.RestoreBackupVolume(task);
            // başarısızsa tekrar dene restore da başarısızsa tekrar dene yok

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
            BackupIncDiffJob._refreshIncDiffTaskFlag = true;
            BackupIncDiffJob._refreshIncDiffLogFlag = true;
        }
    }
}
