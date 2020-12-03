using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler
{
    [ServiceContract]
    public interface ITaskSchedulerManager
    {
        [OperationContract]
        Task InitShedulerAsync();
        [OperationContract]
        string ScheduleRestoreTask(TaskInfo taskInfo);
        [OperationContract]
        string ScheduleBackupTask(TaskInfo taskInfo);
        [OperationContract]
        bool ResumeSchedule(string scheduleId);
        [OperationContract]
        Task ResumeAllScheduleAsync(List<TaskInfo> taskInfoList);
        [OperationContract]
        Task RunNowTrigger(string schedulerId);
        [OperationContract]
        Task DeleteJob(string schedulerId);
        //async Task ResumeAllScheduleAsync(List<TaskInfo> taskInfoList); olmamasına rağmen sorun çıkarmadı biz de kullandık 
        [OperationContract]
        bool PauseSchedule(string scheduleId);
        [OperationContract]
        Task PauseAllScheduleAsync();
        [OperationContract]
        bool EnableSchedule(string scheduleId);
        [OperationContract]
        bool DisableSchedule(string scheduleId);
        [OperationContract]
        bool CancelSchedule(string scheduleId);
        [OperationContract]
        bool CancelAllSchedule(List<TaskInfo> taskInfoList);
        [OperationContract]
        Task RestoreVolumeJob(TaskInfo taskInfo);
        [OperationContract]
        Task RestoreDiskJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupFullNowJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupFullPeriodicHoursJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupFullPeriodicMinutesJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupFullWeeklyJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupFullEverydayJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupFullWeekDaysJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupFullCertainDaysJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupIncDiffNowJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupIncDiffPeriodicHoursJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupIncDiffPeriodicMinutesJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupIncDiffWeeklyJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupIncDiffEverydayJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupIncDiffWeekDaysJob(TaskInfo taskInfo);
        [OperationContract]
        Task BackupIncDiffCertainDaysJob(TaskInfo taskInfo);
        [OperationContract]
        Task RestoreVolumeNowJob(TaskInfo taskInfo);
        [OperationContract]
        Task RestoreDiskNowJob(TaskInfo taskInfo);
    }
}
