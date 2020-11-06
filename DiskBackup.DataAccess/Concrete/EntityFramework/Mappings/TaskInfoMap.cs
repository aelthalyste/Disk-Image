using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class TaskInfoMap : EntityTypeConfiguration<TaskInfo>
    {
        public TaskInfoMap()
        {
            ToTable(@"task_info");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("task_info_id");
            Property(x => x.RestoreTaskId).HasColumnName("restore_task_id");
            Property(x => x.BackupTaskId).HasColumnName("backup_task_id");
            Property(x => x.BackupStorageInfoId).HasColumnName("backup_storage_info_id");
            Property(x => x.StatusInfoId).HasColumnName("status_info_id");
            Property(x => x.Type).HasColumnName("task_type");
            Property(x => x.Name).HasColumnName("name");
            Property(x => x.Obje).HasColumnName("obje");
            Property(x => x.StrObje).HasColumnName("str_obje");
            Property(x => x.Status).HasColumnName("status");
            Property(x => x.CreatedDate).HasColumnName("created_date");
            Property(x => x.NextDate).HasColumnName("next_date");
            Property(x => x.Descripiton).HasColumnName("description");
            Property(x => x.ScheduleId).HasColumnName("scheduler_id");
            Ignore(x => x.RestoreTaskInfo);
            Ignore(x => x.BackupTaskInfo);
            Ignore(x => x.BackupStorageInfo);
            Ignore(x => x.StatusInfo);

        }
    }
}
