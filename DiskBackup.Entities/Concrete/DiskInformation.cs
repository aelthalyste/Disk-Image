using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class DiskInformation : IEntity
    {
        //Görev Oluştur ekranında ve Geri Yükle Ekranında listviewDisk vew listviewRestoreDisk listelerinde
        //2 tabda da progress bar olarak geliyor
        public int DiskId { get; set; } //GUI RestoreDiskTab'ında CW_RestoreToFreshDisk() methodunda kullanılmış
        public long Size { get; set; } //Total Size
        public string StrSize { get; set; } //Total Size
        public List<VolumeInfo> VolumeInfos { get; set; } = new List<VolumeInfo>();
    }
}
