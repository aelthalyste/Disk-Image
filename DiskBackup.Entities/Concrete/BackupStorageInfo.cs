using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class BackupStorageInfo : IEntity
    {
        //Yedekleme alanları & ekle 
        public int Id { get; set; }
        public string StorageName { get; set; }
        public BackupStorageType Type { get; set; } 
        public string Description { get; set; }
        public string StrCapacity { get; set; }
        public long Capacity { get; set; }
        public string StrUsedSize { get; set; }
        public long UsedSize { get; set; }
        public string StrFreeSize { get; set; }
        public long FreeSize { get; set; }
        public string Path { get; set; }
        public bool IsCloud { get; set; }   //true olduğunda modelimiz hybrit oluyor
        public string Domain { get; set; } 
        public string Username { get; set; }
        public string Password { get; set; }
        // Yanında mavi olarak gösterebilmek için ihtiyaç var
        public string StrCloudCapacity { get; set; }
        public string StrCloudUsedSize { get; set; }
        public string StrCloudFreeSize { get; set; }
        public long CloudCapacity { get; set; }
        public long CloudUsedSize { get; set; }
        public long CloudFreeSize { get; set; }
    }

    public enum BackupStorageType
    {
        Windows = 0,
        NAS = 1,
    }
    
}
