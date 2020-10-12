using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DisckBackup.Entities
{
    public class ActivityLog
    {
        public int Id { get; set; }
        public DateTime StartDate { get; set; }
        public DateTime EndDate { get; set; }
        public BackupType backupType { get; set; }
        public TaskInfo TaskName { get; set; } // sistem mi (BackupTask) geri yükle mi (RestoreTask)
        public BackupAreaInfo Target { get; set; } // BackupAreaInofadan AreaName'i gelecek
        public string Status { get; set; }  //Enum yapıp app.xaml'da gösterim anı için dictionary kullanımından vazgeçilebilir....

    }

}
