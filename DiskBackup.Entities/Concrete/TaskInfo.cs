using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class TaskInfo : IEntity
    {
        public int Id { get; set; }
        public int RestoreTaskId { get; set; } // Foreign key
        public int BackupTaskId { get; set; } // Foreign key
        public int BackupStorageInfoId { get; set; } // Foreign key
        public int StatusInfoId { get; set; } // Foreign key
        public RestoreTask RestoreTaskInfo { get; set; }
        public BackupTask BackupTaskInfo { get; set; }
        public BackupStorageInfo BackupStorageInfo { get; set; } // BackupAreaInofadan AreaName'i gelecek
        public StatusInfo StatusInfo { get; set; }
        public TaskType Type { get; set; } // sistem mi (BackupTask) geri yükle mi (RestoreTask)
        public string Name { get; set; }
        public int Obje { get; set; }
        public string StrObje { get; set; } // CDE gibi olması durumunda tutulması
        public TaskStatusType Status { get; set; } //çalışıyor, hazır, iptal edildi...
        public string StrStatus { get; set; }
        public DateTime LastWorkingDate { get; set; } //backuptask starttime ile aynı değil çünkü o zamanlaştırılması daha ileri bir zaman olabilir
        public DateTime NextDate { get; set; }
        public string Descripiton { get; set; }
        public string ScheduleId { get; set; }
        public TecnicalTaskStatusType EnableDisable { get; set; } // 0 Enable - 1 Disable - 2 Broken
    }
    
    public enum TaskType
    {
        Backup = 0, 
        Restore = 1
    }

    public enum TaskStatusType
    {
        FirstMissionExpected = 0,
        Ready = 1,
        Cancel = 2,
        Working = 3,
        Paused = 4,
        Error = 5
    }

    public enum TecnicalTaskStatusType
    {
        Enable = 0,
        Disable = 1,
        Broken = 2
    }
}
