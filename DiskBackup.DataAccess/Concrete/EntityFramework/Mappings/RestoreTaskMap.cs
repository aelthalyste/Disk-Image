using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class RestoreTaskMap : EntityTypeConfiguration<RestoreTask>
    {
        public RestoreTaskMap()
        {
            ToTable(@"restore_task");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("restore_task_id");
            Property(x => x.BackupInfoId).HasColumnName("backup_info_id");
            Property(x => x.DiskId).HasColumnName("disk_id");
            Property(x => x.DiskLetter).HasColumnName("disk_letter");
            Ignore(x => x.BackupInfo);
            Ignore(x => x.DiskInfo);
        }
    }
}
