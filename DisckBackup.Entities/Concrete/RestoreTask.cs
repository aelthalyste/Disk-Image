using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities
{
    public class RestoreTask : IEntity
    {
        // Geri yükle ve yedekleri görüntüle tablarında listViewRestore ve listViewBackups tablolarına ortak veri göndericek
        // Geri Yükleme oluşturuda karşıladık
        public int Id { get; set; }
        public string TaskName { get; set; } // TaskInfo'nun name'i ile aynı olarak tutulacak
        public string Name { get; set; } //dosya adı
        public ulong VolumeSize { get; set; }
        public ulong FileSize { get; set; }
        public string StrVolumeSize { get; set; }
        public string StrFileSize { get; set; }
        public char Letter { get; set; } // aşağıda seçilen volume
    }
    /*
    Batu'dan alınacağı için tutmaya gerek kalmadı
    public List<FileInfo> FileList { get; set; } = new List<FileInfo>(); // FileInfo'ya bağlı (database'e eklemedik)
    public long UsedSize { get; set; }
    public string StrUsedSize { get; set; }
    public bool Bootable { get; set; }
    public BootType BootableType { get; set; }
    public bool Zip { get; set; }
    public string OS { get; set; }
    public string PCName { get; set; }
    public string IpAddress { get; set; }
    public enum BootType
    {
        MBR = 0,
        GPT = 1
    }
    */
}
