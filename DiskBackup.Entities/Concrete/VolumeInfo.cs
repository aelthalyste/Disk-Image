using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class VolumeInfo : IEntity
    {
        //Bootable ekleyebiliriz Batu'da var
        //Görev Oluştur ekranında ve Geri Yükle Ekranında listviewDisk vew listviewRestoreDisk listelerinde
        //2 tabda da progress bar olarak geliyor
        public string DiskName { get; set; } //Disk 1
        public string DiskType { get; set; } //mbr . gpt 
        public bool Bootable { get; set; }
        public string Name { get; set; } //System Reserved ,Local Volume
        public string FileSystem { get; set; } //Dosya Sistemi NTFS
        public long Size { get; set; }
        public string StrSize { get; set; }
        public long FreeSize { get; set; }
        public string StrFreeSize { get; set; }
        public string PrioritySection { get; set; } //Primary ??
        public char Letter { get; set; }
        public HealthSituation HealthStatu { get; set; }
        public string Status { get; set; } // Sağlıklı

    }

    public enum HealthSituation
    {
        Healthy = 0,
        Unhealthy = 1,
    }
}
