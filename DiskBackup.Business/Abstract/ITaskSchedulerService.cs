using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using DiskBackup.Entities.Concrete;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    public interface ITaskSchedulerService
    {
        //Daha değiştirilebilir

        string ScheduleBackupTask(TaskInfo taskInfo);
        string ScheduleRestoreTask(TaskInfo taskInfo);
        
        bool PauseSchedule(string scheduleId);
        bool ResumeSchedule(string scheduleId);
        bool CancelSchedule(string scheduleId);

        bool PauseAllSchedule(List<TaskInfo> taskInfoList);
        bool ResumeAllSchedule(List<TaskInfo> taskInfoList);
        bool CancelAllSchedule(List<TaskInfo> taskInfoList);
    }
}
