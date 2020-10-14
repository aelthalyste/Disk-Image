﻿using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities
{
    public class TaskInfo : IEntity
    {
        public int Id { get; set; }
        public int RestoreTaskId { get; set; } // Foreign key
        public int BackupTaskId { get; set; } // Foreign key
        public int BackupStorageInfoId { get; set; } // Foreign key
        public RestoreTask RestoreTaskInfo { get; set; }
        public BackupTask BackupTaskInfo { get; set; }
        public BackupStorageInfo BackupStorageInfo { get; set; } // BackupAreaInofadan AreaName'i gelecek
        public TaskType Type { get; set; } // sistem mi (BackupTask) geri yükle mi (RestoreTask)
        public string Name { get; set; }
        public int Obje { get; set; }
        public string StrObje { get; set; } // CDE gibi olması durumunda tutulması
        public string Status { get; set; }
        public DateTime CreatedDate { get; set; } //backuptask starttime ile aynı değil çünkü o zamanlaştırılması daha ileri bir zaman olabilir
        public DateTime NextDate { get; set; }
        public string Descripiton { get; set; }
        public string ScheduleId { get; set; }
    }
    
    public enum TaskType
    {
        Backup = 0, 
        Restore = 1
    }
}
