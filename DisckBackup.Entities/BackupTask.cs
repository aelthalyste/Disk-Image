using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DisckBackup.Entities
{
    public class BackupTask
    {
        public int Id { get; set; }
        public TaskInfo TaskName { get; set; } // foreign
        public BackupType Type { get; set; }
        public BackupAreaInfo TargetName { get; set; }
        public int RetentionTime { get; set; }
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
        public DateTime StartTime { get; set; }
        //bitmap gelecek comboBoxlardan itibaren yok
    }

    public enum AutoRunType
    {
        DaysTime = 0,
        WeeklyTime = 1,
        Periodic = 2
    }

    public enum BackupType
    {
        Full = 0,
        Inc = 1,
        Diff = 2
    }
}
