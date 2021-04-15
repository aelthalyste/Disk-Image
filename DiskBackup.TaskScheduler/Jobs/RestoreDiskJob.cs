using DiskBackup.Business.Abstract;
using DiskBackup.Communication;
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
        private IEMailOperations _emailOperations;

        public RestoreDiskJob(IBackupService backupService, ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IRestoreTaskDal restoreTaskDal, IStatusInfoDal statusInfoDal, IActivityLogDal activityLogDal, ILogger logger, IEMailOperations emailOperations)
        {
            _backupService = backupService;
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _restoreTaskDal = restoreTaskDal;
            _statusInfoDal = statusInfoDal;
            _activityLogDal = activityLogDal;
            _logger = logger.ForContext<RestoreDiskJob>();
            _emailOperations = emailOperations;
        }

        public Task Execute(IJobExecutionContext context)
        {
            byte result = 0;
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
                        _logger.Information("{@task} için restore volume görevi çalıştırılamadı. {@letter} volumunde başka görev işliyor.", task, item.StrObje);
                        if (item.Id == task.Id)
                        {
                            if (context.Trigger.GetNextFireTimeUtc() != null)
                                task.NextDate = (context.Trigger.GetNextFireTimeUtc()).Value.LocalDateTime;
                            _taskInfoDal.Update(task);
                            _backupService.RefreshIncDiffTaskFlag(true);
                            throw new JobExecutionException();
                        }
                    }
                }
            }

            try
            {
                task.LastWorkingDate = DateTime.Now;

                if (!workingTask)
                {
                    task.Status = TaskStatusType.Working;
                    _taskInfoDal.Update(task);
                    _backupService.RefreshIncDiffTaskFlag(true);

                    availableResult = _backupService.IsVolumeAvailable(task.RestoreTaskInfo.TargetLetter[0]);

                    if (availableResult)
                    {
                        result = _backupService.RestoreBackupDisk(task);
                        var cleanChainResult = _backupService.CleanChain(task.RestoreTaskInfo.TargetLetter[0]);
                        _logger.Information("{@task} {@value} zinciri temizlendi. Sonuç: {@cleanResult}", task, task.RestoreTaskInfo.TargetLetter[0], cleanChainResult);
                        foreach (var item in task.StrObje)
                        {
                            cleanChainResult = _backupService.CleanChain(item);
                            _logger.Information("{@task} {@value} zinciri temizlendi. Sonuç: {@cleanResult}", task, item, cleanChainResult);
                        }
                    }
                    else
                        _logger.Information("{@task} {@value} volumu müsait değildi", task, task.RestoreTaskInfo.TargetLetter[0]);
                }
                // başarısızsa tekrar dene restore da başarısızsa tekrar dene yok
            }
            catch (Exception e)
            {
                _logger.Error(e, "{@Task} restore disk görevinde hata oluştu.", task);
                result = 0;
            }

            switch(result)
            {
                case 0:
                    _logger.Verbose("{@task} için Restore-Disk görevi bitirildi. Sonuç: NarDIWrapper'dan false geldi.", task);
                    UpdateActivityAndTask(activityLog, task, StatusType.Fail);
                    break;
                case 1:
                    _logger.Verbose("{@task} için Restore-Disk görevi bitirildi. Sonuç: Başarılı.", task);
                    UpdateActivityAndTask(activityLog, task, StatusType.Success);
                    break;
                case 2:
                    _logger.Verbose("{@task} için Restore-Disk görevi bitirildi. Sonuç: Bağlantı hatası.", task);
                    UpdateActivityAndTask(activityLog, task, StatusType.ConnectionError);
                    break;
                case 3:
                    _logger.Verbose("{@task} için Restore-Disk görevi bitirildi. Sonuç: Eksik dosya var.", task);
                    UpdateActivityAndTask(activityLog, task, StatusType.MissingFile);
                    break;
                case 4:
                    _logger.Verbose("{@task} için Restore-Disk görevi bitirildi. Sonuç: Driver initialize edilemedi.", task);
                    UpdateActivityAndTask(activityLog, task, StatusType.DriverNotInitialized);
                    break;
                case 5:
                    _logger.Information("{@task} için Restore-Disk görevi durduruldu.", task);
                    UpdateActivityAndTask(activityLog, task, StatusType.Cancel);
                    break;
                default:
                    _logger.Verbose("{@task} için Restore-Disk görevi bitirildi. Sonuç: Default çalıştı.", task);
                    UpdateActivityAndTask(activityLog, task, StatusType.Fail);
                    break;
            }

            _logger.Information("{@task} için Restore-Disk görevi bitirildi. Sonuç: {@result}.", task, result);
            return Task.CompletedTask; // return değeri kaldırılacak ve async'e çevirilecek burası
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
            if (taskInfo.NextDate < DateTime.Now)
            {
                taskInfo.ScheduleId = "";
                taskInfo.NextDate = Convert.ToDateTime("01/01/0002");
            }
            _logger.Verbose("Yeni SchedulerId: {@newscheduler}", taskInfo.ScheduleId);
            _taskInfoDal.Update(taskInfo);
            _backupService.RefreshIncDiffTaskFlag(true);
            _backupService.RefreshIncDiffLogFlag(true);

            _logger.Verbose("SendEmail çağırılıyor");
            try
            {
                taskInfo.StatusInfo = resultTaskStatusInfo;
                _emailOperations.SendEMail(taskInfo);
            }
            catch (Exception ex)
            {
                _logger.Error(ex + "Email gönderilemedi");
            }
        }
    }
}
