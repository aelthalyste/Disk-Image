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
        public RestoreType Type { get; set; }
        public string TargetLetter { get; set; }
        public string SourceLetter { get; set; }
        public int BackupVersion { get; set; }
        public int DiskId { get; set; }
        public string RootDir { get; set; }
    }

    public enum RestoreType
    {
        RestoreVolume = 0,
        RestoreDisk = 1
    }

}
