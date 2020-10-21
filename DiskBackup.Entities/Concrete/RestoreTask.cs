using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class RestoreTask : IEntity
    {
        // Geri yükle ve yedekleri görüntüle tablarında listViewRestore ve listViewBackups tablolarına ortak veri göndericek
        // Geri Yükleme oluşturuda karşıladık
        public int Id { get; set; }
        public int BackupInfoId { get; set; }
        public BackupInfo BackupInfo { get; set; }
        public DiskInformation DiskInfo { get; set; } //Selected Disk Letter lazım
        public int DiskId { get; set; }
        public string DiskLetter { get; set; }
    }

}
