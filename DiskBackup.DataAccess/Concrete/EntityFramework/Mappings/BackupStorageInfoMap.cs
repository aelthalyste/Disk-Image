using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class BackupStorageInfoMap : EntityTypeConfiguration<BackupStorageInfo>
    {
        public BackupStorageInfoMap()
        {
            ToTable(@"backup_storage");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("backup_storage_id");
            Property(x => x.StorageName).HasColumnName("storage_name");
            Property(x => x.Type).HasColumnName("backup_storage_type");
            Property(x => x.Description).HasColumnName("description");
            Property(x => x.Path).HasColumnName("path");
            Property(x => x.IsCloud).HasColumnName("is_cloud");
            Property(x => x.Domain).HasColumnName("domain");
            Property(x => x.Username).HasColumnName("username");
            Property(x => x.Password).HasColumnName("password");
            Ignore(x => x.Capacity);
            Ignore(x => x.StrCapacity);
            Ignore(x => x.UsedSize);
            Ignore(x => x.StrUsedSize);
            Ignore(x => x.FreeSize);
            Ignore(x => x.StrFreeSize);
            Ignore(x => x.StrCloudCapacity);
            Ignore(x => x.StrCloudFreeSize);
            Ignore(x => x.StrCloudUsedSize);
            Ignore(x => x.CloudCapacity);
            Ignore(x => x.CloudUsedSize);
            Ignore(x => x.CloudFreeSize);

        }
    }
}
