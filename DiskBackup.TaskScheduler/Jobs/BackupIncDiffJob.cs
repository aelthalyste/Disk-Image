﻿using DiskBackup.Business.Abstract;
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
    public class BackupIncDiffJob : IJob
    {
        private readonly IBackupService _backupService;
        private readonly IEntityRepository<TaskInfo> _taskInfoRepository;
        private readonly IEntityRepository<BackupStorageInfo> _backupStorageRepository;
        private readonly IEntityRepository<StatusInfo> _statusInfoRepository;

        public BackupIncDiffJob(IEntityRepository<TaskInfo> taskInfoRepository, IEntityRepository<BackupStorageInfo> backupStorageRepository, IEntityRepository<StatusInfo> statusInfoRepository)
        {
            _backupService = new BackupManager();
            _taskInfoRepository = taskInfoRepository;
            _backupStorageRepository = backupStorageRepository;
            _statusInfoRepository = statusInfoRepository;
        }

        public async Task Execute(IJobExecutionContext context)
        {
            var taskId = (int)context.JobDetail.JobDataMap["taskId"];

            var task = _taskInfoRepository.Get(x => x.Id == taskId);
            task.BackupStorageInfo = _backupStorageRepository.Get(x => x.Id == task.BackupStorageInfoId);

            task.StatusInfo = _statusInfoRepository.Get(x => x.Id == task.StatusInfoId);


            JobExecutionException exception = null;
            bool result = true;

            try
            {
                 result = _backupService.CreateIncDiffBackup(task);
                // activity log burada basılacak
            }
            catch (Exception e)
            {
                if (task.BackupTaskInfo.FailTryAgain)
                {
                    exception = new JobExecutionException(e, context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
                }
            }

            if (!result)
            {
                exception = new JobExecutionException(context.RefireCount <= task.BackupTaskInfo.FailNumberTryAgain);
            }

            if (exception != null)
            {
                await Task.Delay(TimeSpan.FromMinutes(task.BackupTaskInfo.WaitNumberTryAgain));
                throw exception;
            }
        }
    }
}
