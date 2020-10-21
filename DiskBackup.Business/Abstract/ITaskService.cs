using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
                                                                    //BİRİNCİL AMAÇ ÇALIŞAN KOD ÇIKARMAK
    public interface ITaskService
    {
        List<TaskInfo> GetTaskInfoList();

        //RestoreTask parametresi de verebiliriz tam emin olamadık 
        bool AddRestoreTask(TaskInfo taskInfo);
        bool StartRestoreTask(TaskInfo taskInfo);
        bool StopRestoreTask(TaskInfo taskInfo);
        bool PauseRestoreTask(TaskInfo taskInfo); //Restore'da pause işlemi yapılabilir mi 
        bool RemoveRestoreTask(TaskInfo taskInfo);
        bool UpdateRestoreTask(TaskInfo taskInfo);
        bool DisableRestoreTask(TaskInfo taskInfo);
        bool EnableRestoreTask(TaskInfo taskInfo);

        //BackupTask parametresi de verebiliriz tam emin olamadık 
        bool AddBackupTask(TaskInfo taskInfo);
        bool RemoveBackupTask(TaskInfo taskInfo);
        bool PauseBackupTask(TaskInfo taskInfo);
        bool StopBackupTask(TaskInfo taskInfo);
        bool ResumeBackupTask(TaskInfo taskInfo);
        bool UpdateBackupTask(TaskInfo taskInfo);
        bool RestartFailedBackupTask(TaskInfo taskInfo); //Eğer bu methodun neden Restore'da olmadığını düşünüyorsanız NewCreateTask.xaml'da zamanlama tabında cevabı bulacaksınız
        bool DisableBackupTask(TaskInfo taskInfo);
        bool EnableBackupTask(TaskInfo taskInfo);

        //Belki SchedulerManager ile bağlantısı olması gerek
    }
}
