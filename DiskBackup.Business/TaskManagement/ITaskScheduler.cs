using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using DiskBackup.Entities;
using System.Threading.Tasks;

namespace DiskBackup.Business.TaskManagement
{
    public interface ITaskScheduler
    {
        string ScheduleBackupTask(BackupTask backupTask);
        string ScheduleRestoreTask(RestoreTask backupTask);
        bool CancelSchedule(string scheduleId);
    }
}
