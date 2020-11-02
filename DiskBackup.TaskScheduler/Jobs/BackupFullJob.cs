﻿using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
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
    public class BackupFullJob : IJob
    {
        private readonly IBackupService _backupService;
        private readonly IEntityRepository<TaskInfo> _taskInfoRepository;
        private readonly IEntityRepository<BackupStorageInfo> _backupStorageRepository;

        public BackupFullJob(IEntityRepository<TaskInfo> taskInfoRepository, IEntityRepository<BackupStorageInfo> backupStorageRepository)
        {
            _backupService = new BackupManager();
            _taskInfoRepository = taskInfoRepository;
            _backupStorageRepository = backupStorageRepository;
        }

        public Task Execute(IJobExecutionContext context)
        {
            var taskId = (int)context.JobDetail.JobDataMap["taskId"];
            var backupStorageId = (int)context.JobDetail.JobDataMap["backupStorageId"];

            var task = _taskInfoRepository.Get(x => x.Id == taskId);
            var backupStorage = _backupStorageRepository.Get(x => x.Id == backupStorageId);

            var result = _backupService.CreateFullBackup(task, backupStorage);

            // başarısızsa tekrar dene
            // activity log burada basılacak

            return Task.CompletedTask;
        }
    }
}
