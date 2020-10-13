using DisckBackup.Entities;
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
            Property(x => x.StrCapacity).HasColumnName("str_capacity");
            Property(x => x.Capacity).HasColumnName("capacity");
            Property(x => x.StrUsedSize).HasColumnName("str_used_size");
            Property(x => x.UsedSize).HasColumnName("used_size");
            Property(x => x.StrFreeSize).HasColumnName("str_free_size");
            Property(x => x.FreeSize).HasColumnName("free_size");
            Property(x => x.Path).HasColumnName("path");
            Property(x => x.IsCloud).HasColumnName("is_cloud");
            Property(x => x.Domain).HasColumnName("domain");
            Property(x => x.Username).HasColumnName("username");
            Property(x => x.Password).HasColumnName("password");
        }
    }
}
