using DisckBackup.Entities;
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
            Property(x => x.TaskName).HasColumnName("task_name");
            Property(x => x.Name).HasColumnName("name");
            Property(x => x.VolumeSize).HasColumnName("volume_size");
            Property(x => x.FileSize).HasColumnName("file_size");
            Property(x => x.StrVolumeSize).HasColumnName("str_volume_size");
            Property(x => x.StrFileSize).HasColumnName("str_file_size");
            Property(x => x.Letter).HasColumnName("letter");

        }
    }
}
