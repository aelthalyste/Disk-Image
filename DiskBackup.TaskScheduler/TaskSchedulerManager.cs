using DiskBackup.Business.Concrete;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler.Jobs;
using Quartz;
using Quartz.Impl;
using Quartz.Impl.Calendar;
using Quartz.Spi;
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler
{
    public class TaskSchedulerManager
    {
        public IScheduler _scheduler;
        public IJobFactory _jobFactory;
        public static int jobIdCounter = 0;

        public TaskSchedulerManager(IJobFactory jobFactory)
        {
            _jobFactory = jobFactory;
        }

        public async Task InitShedulerAsync()
        {
            NameValueCollection props = new NameValueCollection
                {
                    { "quartz.serializer.type", "binary" }
                };
            StdSchedulerFactory factory = new StdSchedulerFactory(props);
            _scheduler = await factory.GetScheduler();
            _scheduler.JobFactory = _jobFactory;
            await _scheduler.Start();
        }

        #region Backup

        #region Daily
        public async Task BackupIncDiffEverydayJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffEverydayJob_{jobIdCounter}", "Backup")
                .UsingJobData("taskId", taskInfo.Id)
                .UsingJobData("backupStorageId", backupStorageInfo.Id)
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffDailyTrigger_{jobIdCounter}", "Backup")
                .ForJob($"backupIncDiffEverydayJob_{jobIdCounter}" + jobIdCounter)
                .WithSchedule(CronScheduleBuilder.DailyAtHourAndMinute(taskInfo.BackupTaskInfo.StartTime.Hour, taskInfo.BackupTaskInfo.StartTime.Minute)) // execute job daily at 22:00
                .Build();

            await _scheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        public async Task BackupIncDiffWeekDaysJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffWeekDaysJob_{jobIdCounter}", "Backup")
                .UsingJobData("taskId", taskInfo.Id)
                .UsingJobData("backupStorageId", backupStorageInfo.Id)
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffWeekDaysTrigger_{jobIdCounter}", "Backup")
                .WithCronSchedule($"0 {taskInfo.BackupTaskInfo.StartTime.Minute} {taskInfo.BackupTaskInfo.StartTime.Hour}? * MON-FRI") // Haftaiçi günlerinde çalıştırma
                .ForJob($"backupIncDiffWeekDaysJob_{jobIdCounter}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        //yapılmadı*****string çözümü düşünüldü
        public async Task BackupIncDiffCertainDaysJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffCertainDaysJob_{jobIdCounter}", "Backup")
                .UsingJobData("taskId", taskInfo.Id)
                .UsingJobData("backupStorageId", backupStorageInfo.Id)
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffCertainDaysTrigger_{jobIdCounter}", "Backup")
                .WithCronSchedule($"0 {taskInfo.BackupTaskInfo.StartTime.Minute} {taskInfo.BackupTaskInfo.StartTime.Hour}? * {taskInfo.BackupTaskInfo.Days}") // Belirli günler
                .ForJob($"backupIncDiffCertainDaysJob_{jobIdCounter}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        #endregion

        #region Weekly
        public async Task BackupIncDiffWeeklyJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffWeeklyJob_{jobIdCounter}", "Backup")
                .UsingJobData("taskId", taskInfo.Id)
                .UsingJobData("backupStorageId", backupStorageInfo.Id)
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffWeeklyTrigger_{jobIdCounter}", "Backup")
                .WithCronSchedule($"0 {taskInfo.BackupTaskInfo.StartTime.Minute} {taskInfo.BackupTaskInfo.StartTime.Hour}? {taskInfo.BackupTaskInfo.Months} {taskInfo.BackupTaskInfo.Days} # {taskInfo.BackupTaskInfo.WeeklyTime}") // 0 15 10? * 6 # 3
                .ForJob($"backupIncDiffWeeklyJob_{jobIdCounter}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        #endregion

        #region Periodic
        public async Task BackupIncDiffPeriodicHoursJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffPeriodicHoursJob_{jobIdCounter}", "Backup")
                .UsingJobData("taskId", taskInfo.Id)
                .UsingJobData("backupStorageId", backupStorageInfo.Id)
                .Build();

            var trigger = TriggerBuilder.Create()
               .WithIdentity($"backupIncDiffPeriodicHoursTrigger_{jobIdCounter}", "Backup")
               .WithSimpleSchedule(x => x
                   .WithIntervalInHours(taskInfo.BackupTaskInfo.PeriodicTime)
                   .RepeatForever())
               .Build();

            await _scheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        public async Task BackupIncDiffPeriodicMinutesJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffPeriodicHoursJob_{jobIdCounter}", "Backup")
                .UsingJobData("taskId", taskInfo.Id)
                .UsingJobData("backupStorageId", backupStorageInfo.Id)
                .Build();

            var trigger = TriggerBuilder.Create()
               .WithIdentity($"backupIncDiffPeriodicHoursTrigger_{jobIdCounter}", "Backup")
               .WithSimpleSchedule(x => x
                   .WithIntervalInMinutes(taskInfo.BackupTaskInfo.PeriodicTime)
                   .RepeatForever())
               .Build();

            await _scheduler.ScheduleJob(job, trigger);
            jobIdCounter++;
        }

        #endregion

        public async Task BackupIncDiffNowJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreDiskJob>()
                .WithIdentity($"backupIncDiffNowJob_{jobIdCounter}", "Backup")
                .UsingJobData("taskId", taskInfo.Id)
                .UsingJobData("backupStorageId", backupStorageInfo.Id)
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffNowTrigger_{jobIdCounter}", "Backup")
                .ForJob($"backupIncDiffNowJob_{jobIdCounter}", "Backup")
                .StartAt(taskInfo.NextDate) // now yollandığında hemen çalıştıracak
                .Build();

            await _scheduler.ScheduleJob(job, trigger);
            jobIdCounter++;

        }

        #endregion

        #region Restore

        public async Task RestoreDiskJob(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreDiskJob>()
                .WithIdentity($"restoreDiskJob_{jobIdCounter}", "Restore")
                .UsingJobData("taskId", taskInfo.Id)
                .UsingJobData("backupStorageId", backupStorageInfo.Id)
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"restoreDiskTrigger_{jobIdCounter}", "Restore")
                .ForJob($"restoreDiskJob_{jobIdCounter}", "Restore")
                .StartAt(taskInfo.NextDate) // now yollandığında hemen çalıştıracak
                .Build();

            await _scheduler.ScheduleJob(job, trigger);
            jobIdCounter++;

        }

        //return _diskTracker.CW_RestoreToVolume(volumeInfo.Letter, backupInfo.Letter, backupInfo.Version, true, backupInfo.BackupStorageInfo.Path);
        //public bool RestoreBackupVolume(BackupInfo backupInfo, char volumeLetter)

        public async Task RestoreVolumeJob(char volumeLetter, BackupInfo backupInfo, TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreVolumeJob>()
                .WithIdentity($"restoreVolumeJob_{jobIdCounter}", "Restore")
                .UsingJobData("volumeLetter", volumeLetter)
                .UsingJobData("backupInfoId", backupInfo.Id)
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"restoreVolumeTrigger_{jobIdCounter}", "Restore")
                .ForJob($"restoreVolumeJob_{jobIdCounter}", "Restore")
                .StartAt(taskInfo.NextDate) // now yollandığında hemen çalıştıracak
                .Build();

            await _scheduler.ScheduleJob(job, trigger);
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
