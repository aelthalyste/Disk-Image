using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class BackupInfo :IEntity
    {
        //Geri Yükle ve Yedekleri Görüntüle tabında gözükecek model 
        public int Id { get; set; }
        public BackupStorageInfo BackupStorageInfo { get; set; } //BackupTask'ın path gelicek foreignKey
        public int BackupStorageInfoId { get; set; } //foreignKey
        public BackupTypes Type { get; set; }
        public string FileName { get; set; }
        public DateTime CreatedDate { get; set; }
        public string BackupTaskName { get; set; }
        public long VolumeSize { get; set; }
        public string StrVolumeSize { get; set; }
        public long FileSize { get; set; }
        public string StrFileSize { get; set; }
        public string Description { get; set; }
        public bool IsCloud { get; set; }
        public int Version { get; set; } // sor batuya

        //Ignore edilecek, Batu'dan alınacaklar
        public char DiskType { get; set; } //m-mbr, g-gpt
        public int OSVolume { get; set; } //sor batuya
        public char Letter { get; set; } //sürücü harfi
        public long UsedSize { get; set; }
        public string StrUsedSize { get; set; }
        public bool Bootable { get; set; }
        public string Zip { get; set; }
        public string OS { get; set; }
        public string PCName { get; set; }
        public string IpAddress { get; set; }
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
