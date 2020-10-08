using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DisckBackup.Entities
{
    public class DiskInfo
    { 
        //Görev Oluştur ekranında ve Geri Yükle Ekranında listviewDisk vew listviewRestoreDisk listelerinde
        //2 tabda da progress bar olarak geliyor
        public string Name { get; set; } //Disk 1
        public string BootType { get; set; } //mbr . gpt 
        public long Size { get; set; } //Total Size
        public string StrSize { get; set; } //Total Size
        public List<VolumeInfo> VolumeInfos = new List<VolumeInfo>();
    }
}
