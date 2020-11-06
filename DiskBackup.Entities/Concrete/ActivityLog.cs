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
        public int BackupStorageInfoId { get; set; } // foreign key
        public BackupStorageInfo BackupStorageInfo { get; set; } // BackupStorageInfo'dan StorageName
        public StatusInfo StatusInfo { get; set; }
        public string TaskInfoName { get; set; }
        public DateTime StartDate { get; set; }
        public DateTime EndDate { get; set; }
        public StatusType Status { get; set; }  // Enum yapıp app.xaml'da gösterim anı için dictionary kullanımından vazgeçilebilir....
        public string StrStatus { get; set; }

    }

    public enum StatusType
    {
        Success = 0,
        Fail = 1
    }

}
