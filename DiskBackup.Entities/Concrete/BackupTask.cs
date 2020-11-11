using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class BackupTask : IEntity
    {
        public int Id { get; set; }
        public string TaskName { get; set; } // TaskInfo'nun name'i ile aynı olarak tutulacak
        public BackupTypes Type { get; set; }
        public int RetentionTime { get; set; } // saklama süresi
        public bool FullOverwrite { get; set; }
        public int FullBackup { get; set; }
        public int NarRetentionTime { get; set; }
        public bool NarFullOverwrite { get; set; }
        public int NarFullBackup { get; set; }
        public bool AutoRun { get; set; } //false ise manuel çalışacak
        public bool FailTryAgain { get; set; } // true ise FailNumberTryAgain'e bak
        public int FailNumberTryAgain { get; set; }
        public int WaitNumberTryAgain { get; set; } //her denemeden önce bekle
        public AutoRunType AutoType { get; set; }
        //public DateTime StartTime { get; set; } //zamanlanmış görevin başlangıç saati
        public string Days { get; set; } // bu böyle olacak günlerin set edildiği yerde ise foreach ile atama sağlanacak 0,1,2,3,4,5,6 cron için
        public string Months { get; set; } // 1,2,3,4,5,6,7,8,9,10,11,12
        public WeeklyType WeeklyTime { get; set; } 
        public PeriodicType PeriodicTimeType { get; set; }
        public int PeriodicTime { get; set; }
    }

    public enum BackupTypes
    {
        Diff = 0,
        Inc = 1, 
        Full = 2
    }

    public enum AutoRunType
    {
        DaysTime = 0,
        WeeklyTime = 1,
        Periodic = 2
    }

    public enum WeeklyType
    {
        First = 1,
        Second = 2,
        Third = 3,
        Fourth = 4
    }

    public enum PeriodicType
    {
        Minute = 0,
        Hour = 1,
    }

    //public enum DayMask
    //{
    //    Monday = 1,
    //    Tuesday = 2,
    //    Wednesday = 4,
    //    Thursday = 8,
    //    Friday = 16,
    //    Saturday = 32,
    //    Sunday = 64
    //}
}
