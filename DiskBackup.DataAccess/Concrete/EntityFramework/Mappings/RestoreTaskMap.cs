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
            Property(x => x.Type).HasColumnName("type");
            Property(x => x.BackupVersion).HasColumnName("backup_version");
            Property(x => x.DiskId).HasColumnName("disk_id");
            Property(x => x.RootDir).HasColumnName("root_dir");
            Property(x => x.TargetLetter).HasColumnName("target_letter");
            Property(x => x.SourceLetter).HasColumnName("source_letter");
            Property(x => x.Bootable).HasColumnName("bootable");

        }
    }
}
