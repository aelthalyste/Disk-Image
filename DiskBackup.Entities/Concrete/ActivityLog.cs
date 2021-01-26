using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class ActivityLog : IEntity
    {
        public int Id { get; set; }
        public int StatusInfoId { get; set; } // foreign key
        public StatusInfo StatusInfo { get; set; }
        public string TaskInfoName { get; set; }
        public DateTime StartDate { get; set; }
        public DateTime EndDate { get; set; }
        //public StatusType Status { get; set; }  // Enum yapıp app.xaml'da gösterim anı için dictionary kullanımından vazgeçilebilir....
        //public string StrStatus { get; set; }
        public string BackupStoragePath { get; set; }
        public DetailedMissionType Type { get; set; }

    }

    public enum StatusType
    {
        Success = 0,
        Fail = 1,
        Cancel = 2,
        NotEnoughDiskSpace = 3,
        ConnectionError = 4,
        MissingFile = 5
    }

    public enum DetailedMissionType
    {
        Diff = 0,
        Inc = 1,
        Full = 2,
        Restore = 3
    }

}
