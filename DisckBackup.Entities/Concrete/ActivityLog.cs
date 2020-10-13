using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DisckBackup.Entities
{
    public class ActivityLog : IEntity
    {
        public int Id { get; set; }
        public DateTime StartDate { get; set; }
        public DateTime EndDate { get; set; }
        public BackupTypes BackupType { get; set; }
        public TaskInfo TaskName { get; set; } // sistem mi (BackupTask) geri yükle mi (RestoreTask)
        public BackupStorageInfo Target { get; set; } // BackupAreaInofadan AreaName'i gelecek
        public StatusType Status { get; set; }  //Enum yapıp app.xaml'da gösterim anı için dictionary kullanımından vazgeçilebilir....
        public string StrStatus { get; set; }
    }

    public enum StatusType
    {
        Success = 0,
        Fail = 1
    }

}
