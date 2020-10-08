using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DisckBackup.Entities
{
    public class TaskInfo
    {
        public int Id { get; set; }
        public int TargetId { get; set; } // BackupAreaInofadan AreaName'i gelecek
        public bool Type { get; set; } // sistem mi (BackupTask) geri yükle mi (RestoreTask)
        public string Name { get; set; }
        public int Obje { get; set; }
        public string Status { get; set; }
        public DateTime CreatedDate { get; set; }
        public DateTime NextDate { get; set; }
        public string Descripiton { get; set; }
    }
}
