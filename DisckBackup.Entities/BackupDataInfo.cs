using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DisckBackup.Entities
{
    public class BackupDataInfo
    {
        //Geri yükle ve yedekleri görüntüle tablarında listViewRestore ve listViewBackups tablolarına ortak veri göndericek
        public string Type { get; set; }
        public string Name { get; set; }
        public string CreatedDate { get; set; }
        public string TaskName { get; set; }
        public long VolumeSize { get; set; }
        public long FileSize { get; set; }
        public string StrVolumeSize { get; set; }
        public string StrFileSize { get; set; }
        public char Letter { get; set; }
        public string Descripiton { get; set; }
        public long UsedSize { get; set; }
        public string StrUsedSize { get; set; }
        public bool Bootable { get; set; }
        public bool Zip { get; set; }
        public string OS { get; set; }
        public string PCName { get; set; }
        public string IpAddress { get; set; }
    }
}
