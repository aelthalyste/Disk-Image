using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class BackupTaskMap: EntityTypeConfiguration<BackupTask>
    {
        public BackupTaskMap()
        {
            ToTable(@"backup_task");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("backup_task_id");
            Property(x => x.TaskName).HasColumnName("task_name");
            Property(x => x.Type).HasColumnName("backup_type");
            Property(x => x.RetentionTime).HasColumnName("retention_time");
            Property(x => x.FullOverwrite).HasColumnName("full_overwrite");
            Property(x => x.FullBackup).HasColumnName("full_backup");
            Property(x => x.NarRetentionTime).HasColumnName("nar_retention_time");
            Property(x => x.NarFullOverwrite).HasColumnName("nar_full_overwrite");
            Property(x => x.NarFullBackup).HasColumnName("nar_full_backup");
            Property(x => x.AutoRun).HasColumnName("auto_run");
            Property(x => x.FailTryAgain).HasColumnName("fail_try_again");
            Property(x => x.FailNumberTryAgain).HasColumnName("fail_number_try_again");
            Property(x => x.WaitNumberTryAgain).HasColumnName("wait_number_try_again");
            Property(x => x.AutoType).HasColumnName("auto_run_type");
            //Property(x => x.StartTime).HasColumnName("start_time");
            Property(x => x.Days).HasColumnName("days");
            Property(x => x.Months).HasColumnName("months");
            Property(x => x.WeeklyTime).HasColumnName("weekly_time");
            Property(x => x.PeriodicTime).HasColumnName("periodic_time");
            Property(x => x.PeriodicTimeType).HasColumnName("periodic_time_type");
        }
    }
}
