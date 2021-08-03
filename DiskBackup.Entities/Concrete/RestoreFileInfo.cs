using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class RestoreFileInfo
    {
        public string FileName { get; set; }
        public FileRestoreResult Status { get; set; }
        public string StrStatus { get; set; }
    }

    public enum FileRestoreResult
    {
        RestoreFailed = 0,
        RestoreSuccessful = 1,
        RestoreFailedToStart = 2,
        Fail = 3
    }
}
