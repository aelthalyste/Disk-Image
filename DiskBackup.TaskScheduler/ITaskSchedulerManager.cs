using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler
{
    public interface ITaskSchedulerManager
    {
        Task InitShedulerAsync();
        string ScheduleRestoreTask(TaskInfo taskInfo);
        string ScheduleBackupTask(TaskInfo taskInfo);
        bool ResumeSchedule(string scheduleId);
        Task ResumeAllScheduleAsync(List<TaskInfo> taskInfoList);
        //async Task ResumeAllScheduleAsync(List<TaskInfo> taskInfoList); olmamasına rağmen sorun çıkarmadı biz de kullandık 
        bool PauseSchedule(string scheduleId);
        Task PauseAllScheduleAsync();
        bool EnableSchedule(string scheduleId);
        bool DisableSchedule(string scheduleId);
        bool CancelSchedule(string scheduleId);
        bool CancelAllSchedule(List<TaskInfo> taskInfoList);
        Task RestoreVolumeJob(char volumeLetter, BackupInfo backupInfo, TaskInfo taskInfo);
        Task RestoreDiskJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo);
        Task BackupFullNowJob(TaskInfo taskInfo);
        Task BackupFullPeriodicHoursJob(TaskInfo taskInfo);
        Task BackupFullPeriodicMinutesJob(TaskInfo taskInfo);
        Task BackupFullWeeklyJob(TaskInfo taskInfo);
        Task BackupFullEverydayJob(TaskInfo taskInfo);
        Task BackupFullWeekDaysJob(TaskInfo taskInfo);
        Task BackupFullCertainDaysJob(TaskInfo taskInfo);
        Task BackupIncDiffNowJob(TaskInfo taskInfo);
        Task BackupIncDiffPeriodicHoursJob(TaskInfo taskInfo);
        Task BackupIncDiffPeriodicMinutesJob(TaskInfo taskInfo);
        Task BackupIncDiffWeeklyJob(TaskInfo taskInfo);
        Task BackupIncDiffEverydayJob(TaskInfo taskInfo);
        Task BackupIncDiffWeekDaysJob(TaskInfo taskInfo);
        Task BackupIncDiffCertainDaysJob(TaskInfo taskInfo);

    }
}
