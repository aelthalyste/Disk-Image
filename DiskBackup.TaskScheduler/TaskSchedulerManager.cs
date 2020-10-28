using DiskBackup.Business.Concrete;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler.Jobs;
using Quartz;
using Quartz.Impl.Calendar;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler
{
    public class TaskSchedulerManager
    {
        public IScheduler MyScheduler { get; set; }
        public static int jobIdCounter = 0;
        public static TaskInfo _taskInfo = null;
        public static BackupStorageInfo _backupStorageInfo = null;
        public static BackupManager _backupManager = null;

        public async Task CreateBackupIncDiffJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            _taskInfo = taskInfo;
            _backupStorageInfo = backupStorageInfo;

            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
               .WithIdentity("job3" + jobIdCounter, "group3")
               .Build();
            var trigger = TriggerBuilder.Create()
               .WithIdentity("trigger3" + jobIdCounter, "group3")
               .WithSimpleSchedule(x => x
                   .WithIntervalInMinutes(1)
                   .WithRepeatCount(0))
               .Build();
            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        //*************************************************************************************************

        #region Backup

        #region Daily
        public async Task BackupIncDiffEverydayJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity("backupIncDiffEverydayJob_" + jobIdCounter, "Backup")
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("backupIncDiffDailyTrigger_" + jobIdCounter, "Backup")
                .ForJob("backupIncDiffEverydayJob_" + jobIdCounter)
                .WithSchedule(CronScheduleBuilder.DailyAtHourAndMinute(taskInfo.BackupTaskInfo.StartTime.Hour, taskInfo.BackupTaskInfo.StartTime.Minute)) // execute job daily at 22:00
                .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        public async Task BackupIncDiffWeekDaysJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity("backupIncDiffWeekDaysJob_" + jobIdCounter, "Backup")
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("backupIncDiffWeekDaysTrigger_", "Backup")
                .WithCronSchedule($"0 {taskInfo.BackupTaskInfo.StartTime.Minute} {taskInfo.BackupTaskInfo.StartTime.Hour}? * MON-FRI") // Haftaiçi günlerinde çalıştırma
                .ForJob("backupIncDiffWeekDaysJob_", "Backup")
                .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        //yapılmadı*****string çözümü düşünüldü
        public async Task BackupIncDiffCertainDaysJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity("backupIncDiffCertainDaysJob_" + jobIdCounter, "Backup")
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("backupIncDiffCertainDaysTrigger_", "Backup")
                .WithCronSchedule($"0 {taskInfo.BackupTaskInfo.StartTime.Minute} {taskInfo.BackupTaskInfo.StartTime.Hour}? * {taskInfo.BackupTaskInfo.Days}") // Belirli günler
                .ForJob("backupIncDiffCertainDaysJob_", "Backup")
                .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        #endregion

        #region Weekly
        public async Task BackupIncDiffWeeklyJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity("backupIncDiffWeeklyJob_" + jobIdCounter, "Backup")
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("backupIncDiffWeeklyTrigger_", "Backup")
                .WithCronSchedule($"0 {taskInfo.BackupTaskInfo.StartTime.Minute} {taskInfo.BackupTaskInfo.StartTime.Hour}? {taskInfo.BackupTaskInfo.Months} {taskInfo.BackupTaskInfo.Days} # {taskInfo.BackupTaskInfo.WeeklyTime}") // 0 15 10? * 6 # 3
                .ForJob("backupIncDiffWeeklyJob_", "Backup")
                .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        #endregion

        #region Periodic
        public async Task BackupIncDiffPeriodicHoursJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity("backupIncDiffPeriodicHoursJob_" + jobIdCounter, "Backup")
                .Build();

            var trigger = TriggerBuilder.Create()
               .WithIdentity("backupIncDiffPeriodicHoursTrigger_" + jobIdCounter, "Backup")
               .WithSimpleSchedule(x => x
                   .WithIntervalInHours(taskInfo.BackupTaskInfo.PeriodicTime)
                   .RepeatForever())
               .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        public async Task BackupIncDiffPeriodicMinutesJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity("backupIncDiffPeriodicHoursJob_" + jobIdCounter, "Backup")
                .Build();

            var trigger = TriggerBuilder.Create()
               .WithIdentity("backupIncDiffPeriodicHoursTrigger_" + jobIdCounter, "Backup")
               .WithSimpleSchedule(x => x
                   .WithIntervalInMinutes(taskInfo.BackupTaskInfo.PeriodicTime)
                   .RepeatForever())
               .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        #endregion

        public async Task BackupIncDiffNowJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreDiskJob>()
                .WithIdentity("backupIncDiffNowJob_" + jobIdCounter, "Backup")
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("backupIncDiffNowTrigger_" + jobIdCounter, "Backup")
                .ForJob("restoreDiskJob_" + jobIdCounter)
                .StartAt(taskInfo.NextDate) // now yollandığında hemen çalıştıracak
                .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;

        }

        #endregion

        #region Restore

        public async Task RestoreDiskJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreDiskJob>()
                .WithIdentity("restoreDiskJob_" + jobIdCounter, "Restore")
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("restoreDiskTrigger_" + jobIdCounter, "Restore")
                .ForJob("restoreDiskJob_" + jobIdCounter)
                .StartAt(taskInfo.NextDate) // now yollandığında hemen çalıştıracak
                .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;

        }

        public async Task RestoreVolumeJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreVolumeJob>()
                .WithIdentity("restoreVolumeJob_" + jobIdCounter, "Restore")
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("restoreVolumeTrigger_" + jobIdCounter, "Restore")
                .ForJob("restoreVolumeJob_" + jobIdCounter)
                .StartAt(taskInfo.NextDate) // now yollandığında hemen çalıştıracak
                .Build();

            await MyScheduler.ScheduleJob(job, trigger);
            jobIdCounter++;

        }

        #endregion




































        public bool CancelAllSchedule(List<TaskInfo> taskInfoList)
        {
            throw new NotImplementedException();
        }

        public bool CancelSchedule(string scheduleId)
        {
            throw new NotImplementedException();
        }

        public bool DisableSchedule(string scheduleId)
        {
            throw new NotImplementedException();
        }

        public bool EnableSchedule(string scheduleId)
        {
            throw new NotImplementedException();
        }

        public bool PauseAllSchedule(List<TaskInfo> taskInfoList)
        {
            throw new NotImplementedException();
        }

        public bool PauseSchedule(string scheduleId)
        {
            throw new NotImplementedException();
        }

        public bool ResumeAllSchedule(List<TaskInfo> taskInfoList)
        {
            throw new NotImplementedException();
        }

        public bool ResumeSchedule(string scheduleId)
        {
            throw new NotImplementedException();
        }

        public string ScheduleBackupTask(TaskInfo taskInfo)
        {
            throw new NotImplementedException();
        }

        public string ScheduleRestoreTask(TaskInfo taskInfo)
        {
            throw new NotImplementedException();
        }
    }
}
