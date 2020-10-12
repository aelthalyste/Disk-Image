using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DisckBackup.Entities
{
    public class RestoreTask : IEntity
    {
        // Geri yükle ve yedekleri görüntüle tablarında listViewRestore ve listViewBackups tablolarına ortak veri göndericek
        // Geri Yükleme oluşturuda karşıladık
        // görevlerde de yer alacak dolaylı
        public int Id { get; set; }
        public TaskInfo TaskName { get; set; }
        public List<FileInfo> FileList { get; set; } = new List<FileInfo>(); // FileInfo'ya bağlı (database'e eklemedik)
        public string Name { get; set; } //dosya adı
        public long VolumeSize { get; set; }
        public long FileSize { get; set; }
        public string StrVolumeSize { get; set; }
        public string StrFileSize { get; set; }
        public char Letter { get; set; }
        public long UsedSize { get; set; }
        public string StrUsedSize { get; set; }
        public bool Bootable { get; set; }
        public BootType BootableType { get; set; }
        public bool Zip { get; set; }
        public string OS { get; set; }
        public string PCName { get; set; }
        public string IpAddress { get; set; }
    }

    public enum BootType
    {
        MBR = 0,
        GPT = 1
    }

}
