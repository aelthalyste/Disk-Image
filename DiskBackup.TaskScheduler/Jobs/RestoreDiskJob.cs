﻿using Quartz;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler.Jobs
{
    public class RestoreDiskJob : IJob
    {
        public Task Execute(IJobExecutionContext context)
        {
            throw new NotImplementedException();
        }
    }
}