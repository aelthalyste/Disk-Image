using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class BackupInfoMap : EntityTypeConfiguration<BackupInfo>
    {
        public BackupInfoMap()
        {
            ToTable(@"backup_info");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("backup_info_id");
            Property(x => x.BackupStorageInfoId).HasColumnName("backup_storage_info_id");
            Property(x => x.Type).HasColumnName("backup_types");
            Property(x => x.FileName).HasColumnName("file_name");
            Property(x => x.CreatedDate).HasColumnName("created_date");
            Property(x => x.BackupTaskName).HasColumnName("backup_task_name");
            Property(x => x.VolumeSize).HasColumnName("volume_size");
            Property(x => x.StrVolumeSize).HasColumnName("str_volume_size");
            Property(x => x.FileSize).HasColumnName("file_size");
            Property(x => x.StrFileSize).HasColumnName("str_file_size");
            Property(x => x.Description).HasColumnName("description");
            Property(x => x.IsCloud).HasColumnName("is_cloud");
            Ignore(x => x.BackupStorageInfo);
        }
    }
}
