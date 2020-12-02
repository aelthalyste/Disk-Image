using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler.Jobs;
using Quartz;
using Quartz.Impl;
using Quartz.Impl.Calendar;
using Quartz.Spi;
using Serilog;
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler
{
    [ServiceBehavior(InstanceContextMode = InstanceContextMode.Single)]
    public class TaskSchedulerManager : ITaskSchedulerManager
    {
        public IScheduler _scheduler;
        public IJobFactory _jobFactory;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly ILogger _logger;

        public TaskSchedulerManager(IJobFactory jobFactory, ITaskInfoDal taskInfoDal, ILogger logger)
        {
            _jobFactory = jobFactory;
            _taskInfoDal = taskInfoDal;
            _logger = logger.ForContext<TaskSchedulerManager>();
        }

        public async Task InitShedulerAsync()
        {
            StdSchedulerFactory factory = new StdSchedulerFactory();
            _scheduler = await factory.GetScheduler();
            _scheduler.JobFactory = _jobFactory;
            await _scheduler.Start();
        }

        public async Task RunNowTrigger(string schedulerId)
        {
            var result = schedulerId.Split('/');
            await _scheduler.TriggerJob(new JobKey(result[0], result[1]));
        }

        public async Task DeleteJob(string schedulerId)
        {
            var result = schedulerId.Split('/');
            await _scheduler.DeleteJob(new JobKey(result[0], result[1]));
        }

        #region Backup Inc-Diff

        #region Daily
        public async Task BackupIncDiffEverydayJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffEverydayJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffDailyTrigger_{taskInfo.Id}", "Backup")
                .ForJob($"backupIncDiffEverydayJob_{taskInfo.Id}", "Backup")
                .WithSchedule(CronScheduleBuilder.DailyAtHourAndMinute(taskInfo.NextDate.Hour, taskInfo.NextDate.Minute)) // execute job daily at 22:00
                .Build();

            await _scheduler.ScheduleJob(job, trigger);
            Console.WriteLine("Task: "+taskInfo.NextDate.Hour + " saat" + taskInfo.NextDate.Minute + " dakika" + taskInfo.NextDate);
            taskInfo.ScheduleId = $"backupIncDiffEverydayJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
            
        }

        public async Task BackupIncDiffWeekDaysJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffWeekDaysJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffWeekDaysTrigger_{taskInfo.Id}", "Backup")
                .WithCronSchedule($"0 {taskInfo.NextDate.Minute} {taskInfo.NextDate.Hour} ? * MON-FRI") // Haftaiçi günlerinde çalıştırma
                .ForJob($"backupIncDiffWeekDaysJob_{taskInfo.Id}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupIncDiffWeekDaysJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        //string çözümü düşünüldü
        public async Task BackupIncDiffCertainDaysJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffCertainDaysJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffCertainDaysTrigger_{taskInfo.Id}", "Backup")
                .WithCronSchedule($"0 {taskInfo.NextDate.Minute} {taskInfo.NextDate.Hour} ? * {taskInfo.BackupTaskInfo.Days}") // Belirli günler
                .ForJob($"backupIncDiffCertainDaysJob_{taskInfo.Id}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupIncDiffCertainDaysJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        #endregion

        #region Weekly
        public async Task BackupIncDiffWeeklyJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffWeeklyJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffWeeklyTrigger_{taskInfo.Id}", "Backup")
                .WithCronSchedule($"0 {taskInfo.NextDate.Minute} {taskInfo.NextDate.Hour} ? {taskInfo.BackupTaskInfo.Months} {taskInfo.BackupTaskInfo.Days}#{(int)(taskInfo.BackupTaskInfo.WeeklyTime)}") // 0 15 10 ? * 6#3
                .ForJob($"backupIncDiffWeeklyJob_{taskInfo.Id}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupIncDiffWeeklyJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        #endregion

        #region Periodic
        public async Task BackupIncDiffPeriodicHoursJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffPeriodicHoursJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            var trigger = TriggerBuilder.Create()
               .WithIdentity($"backupIncDiffPeriodicHoursTrigger_{taskInfo.Id}", "Backup")
               .WithSimpleSchedule(x => x
                   .WithIntervalInHours(taskInfo.BackupTaskInfo.PeriodicTime)
                   .RepeatForever())
               .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupIncDiffPeriodicHoursJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        public async Task BackupIncDiffPeriodicMinutesJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffPeriodicHoursJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            var trigger = TriggerBuilder.Create()
               .WithIdentity($"backupIncDiffPeriodicHoursTrigger_{taskInfo.Id}", "Backup")
               .WithSimpleSchedule(x => x
                   .WithIntervalInMinutes(taskInfo.BackupTaskInfo.PeriodicTime)
                   .RepeatForever())
               .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupIncDiffPeriodicHoursJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        #endregion

        public async Task BackupIncDiffNowJob(TaskInfo taskInfo)
        {
            Console.WriteLine("job'ın üzeri");
            IJobDetail job = JobBuilder.Create<BackupIncDiffJob>()
                .WithIdentity($"backupIncDiffNowJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();
            Console.WriteLine("triggerin üzeri");
            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupIncDiffNowTrigger_{taskInfo.Id}", "Backup")
                .WithSimpleSchedule(x => x.WithIntervalInSeconds(20).WithRepeatCount(0))
                .Build();

            taskInfo.ScheduleId = taskInfo.ScheduleId + $"*backupIncDiffNowJob_{taskInfo.Id}-Backup";
            _taskInfoDal.Update(taskInfo);

            await _scheduler.ScheduleJob(job, trigger);
        }

        #endregion

        #region Backup Full

        #region Daily
        public async Task BackupFullEverydayJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupFullJob>()
                .WithIdentity($"backupFullEverydayJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupFullDailyTrigger_{taskInfo.Id}", "Backup")
                .ForJob($"backupFullEverydayJob_{taskInfo.Id}", "Backup")
                .WithSchedule(CronScheduleBuilder.DailyAtHourAndMinute(taskInfo.NextDate.Hour, taskInfo.NextDate.Minute)) // execute job daily at 22:00
                .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupFullEverydayJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        public async Task BackupFullWeekDaysJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupFullJob>()
                .WithIdentity($"backupFullWeekDaysJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupFullWeekDaysTrigger_{taskInfo.Id}", "Backup")
                .WithCronSchedule($"0 {taskInfo.NextDate.Minute} {taskInfo.NextDate.Hour} ? * MON-FRI") // Haftaiçi günlerinde çalıştırma
                .ForJob($"backupFullWeekDaysJob_{taskInfo.Id}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupFullWeekDaysJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        //string çözümü düşünüldü
        public async Task BackupFullCertainDaysJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupFullJob>()
                .WithIdentity($"backupFullCertainDaysJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupFullCertainDaysTrigger_{taskInfo.Id}", "Backup")
                .WithCronSchedule($"0 {taskInfo.NextDate.Minute} {taskInfo.NextDate.Hour} ? * {taskInfo.BackupTaskInfo.Days}") // Belirli günler
                .ForJob($"backupFullCertainDaysJob_{taskInfo.Id}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupFullCertainDaysJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        #endregion

        #region Weekly
        public async Task BackupFullWeeklyJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupFullJob>()
                .WithIdentity($"backupFullWeeklyJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupFullWeeklyTrigger_{taskInfo.Id}", "Backup")
                .WithCronSchedule($"0 {taskInfo.NextDate.Minute} {taskInfo.NextDate.Hour} ? {taskInfo.BackupTaskInfo.Months} {taskInfo.BackupTaskInfo.Days}#{(int)(taskInfo.BackupTaskInfo.WeeklyTime)}") // 0 15 10 ? * 6 # 3
                .ForJob($"backupFullWeeklyJob_{taskInfo.Id}", "Backup")
                .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupFullWeeklyJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        #endregion

        #region Periodic
        public async Task BackupFullPeriodicHoursJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupFullJob>()
                .WithIdentity($"backupFullPeriodicHoursJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            var trigger = TriggerBuilder.Create()
               .WithIdentity($"backupFullPeriodicHoursTrigger_{taskInfo.Id}", "Backup")
               .WithSimpleSchedule(x => x
                   .WithIntervalInHours(taskInfo.BackupTaskInfo.PeriodicTime)
                   .RepeatForever())
               .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupFullPeriodicHoursJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        public async Task BackupFullPeriodicMinutesJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupFullJob>()
                .WithIdentity($"backupFullPeriodicMinutesJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            var trigger = TriggerBuilder.Create()
               .WithIdentity($"backupFullPeriodicMinutesTrigger_{taskInfo.Id}", "Backup")
               .WithSimpleSchedule(x => x
                   .WithIntervalInMinutes(taskInfo.BackupTaskInfo.PeriodicTime)
                   .RepeatForever())
               .Build();

            await _scheduler.ScheduleJob(job, trigger);

            taskInfo.ScheduleId = $"backupFullPeriodicMinutesJob_{taskInfo.Id}/Backup";
            taskInfo.NextDate = trigger.GetNextFireTimeUtc().Value.LocalDateTime;
            _taskInfoDal.Update(taskInfo);
        }

        #endregion

        public async Task BackupFullNowJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<BackupFullJob>()
                .WithIdentity($"backupFullNowJob_{taskInfo.Id}", "Backup")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"backupFullNowTrigger_{taskInfo.Id}", "Backup")
                .ForJob($"backupFullNowJob_{taskInfo.Id}", "Backup")
                .StartAt(taskInfo.NextDate) // now yollandığında hemen çalıştıracak
                .Build();

            taskInfo.ScheduleId = taskInfo.ScheduleId + $"*backupFullNowJob_{taskInfo.Id}-Backup";
            _taskInfoDal.Update(taskInfo);

            await _scheduler.ScheduleJob(job, trigger);
        }

        #endregion

        #region Restore

        public async Task RestoreDiskJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreDiskJob>()
                .WithIdentity($"restoreDiskJob_{taskInfo.Id}", "Restore")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"restoreDiskTrigger_{taskInfo.Id}", "Restore")
                .ForJob($"restoreDiskJob_{taskInfo.Id}", "Restore")
                .StartAt(taskInfo.NextDate)
                .Build();

            taskInfo.ScheduleId = $"restoreDiskJob_{taskInfo.Id}/Restore";
            _taskInfoDal.Update(taskInfo);

            await _scheduler.ScheduleJob(job, trigger);
        }

        public async Task RestoreDiskNowJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreDiskJob>()
                .WithIdentity($"restoreDiskNowJob_{taskInfo.Id}", "Restore")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"restoreDiskNowTrigger_{taskInfo.Id}", "Restore")
                .ForJob($"restoreDiskNowJob_{taskInfo.Id}", "Restore")
                .StartNow() 
                .Build();

            taskInfo.ScheduleId = taskInfo.ScheduleId + $"*restoreDiskNowJob_{taskInfo.Id}/Restore";
            _taskInfoDal.Update(taskInfo);

            await _scheduler.ScheduleJob(job, trigger);
        }


        //return _diskTracker.CW_RestoreToVolume(volumeInfo.Letter, backupInfo.Letter, backupInfo.Version, true, backupInfo.BackupStorageInfo.Path);
        //public bool RestoreBackupVolume(BackupInfo backupInfo, char volumeLetter)

        public async Task RestoreVolumeJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreVolumeJob>()
                .WithIdentity($"restoreVolumeJob_{taskInfo.Id}", "Restore")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"restoreVolumeTrigger_{taskInfo.Id}", "Restore")
                .ForJob($"restoreVolumeJob_{taskInfo.Id}", "Restore")
                .StartAt(taskInfo.NextDate) // now yollandığında hemen çalıştıracak // Sorun çıkması durumunda startAt değişecek
                .Build();

            taskInfo.ScheduleId = $"restoreVolumeJob_{taskInfo.Id}/Restore";
            _taskInfoDal.Update(taskInfo);

            await _scheduler.ScheduleJob(job, trigger);

        }

        public async Task RestoreVolumeNowJob(TaskInfo taskInfo)
        {
            IJobDetail job = JobBuilder.Create<RestoreVolumeJob>()
                .WithIdentity($"restoreVolumeNowJob_{taskInfo.Id}", "Restore")
                .UsingJobData("taskId", taskInfo.Id.ToString())
                .Build();

            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity($"restoreVolumeNowTrigger_{taskInfo.Id}", "Restore")
                .ForJob($"restoreVolumeNowJob_{taskInfo.Id}", "Restore")
                .StartNow()
                .Build();

            taskInfo.ScheduleId = taskInfo.ScheduleId + $"*restoreVolumeNowJob_{taskInfo.Id}/Restore";
            _taskInfoDal.Update(taskInfo);

            await _scheduler.ScheduleJob(job, trigger);

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

        public async Task PauseAllScheduleAsync()
        {
            await _scheduler.PauseAll();
        }

        public bool PauseSchedule(string scheduleId)
        {
            throw new NotImplementedException();
        }

        public async Task ResumeAllScheduleAsync(List<TaskInfo> taskInfoList)
        {
            await _scheduler.ResumeAll();
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
