using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class TaskSchedulerManager
    {
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
