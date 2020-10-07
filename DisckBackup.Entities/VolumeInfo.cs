using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DisckBackup.Entities
{
    public class VolumeInfo
    {
        //Görev Oluştur ekranında ve Geri Yükle Ekranında listviewDisk vew listviewRestoreDisk listelerinde
        //2 tabda da progress bar olarak geliyor
        public string Name { get; set; } //System Reserved ,Local Volume
        public string Format { get; set; } //Dosya Sistemi NTFS
        public long Size { get; set; }
        public string StrSize { get; set; }
        public long FreeSize { get; set; }
        public string StrFreeSize { get; set; }
        public string PrioritySection { get; set; } //Primary ??
        public char Letter { get; set; }
        public string Status { get; set; } // Sağlıklı

        //hem string hem long tutsak nasıl olur size & freeSize  ??
    }
}
