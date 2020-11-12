using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class ActivitiyLogMap : EntityTypeConfiguration<ActivityLog>
    {
        public ActivitiyLogMap()
        {
            ToTable(@"activitiy_log");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("activitiy_log_id");
            Property(x => x.StatusInfoId).HasColumnName("status_info_id");
            Property(x => x.BackupStorageInfoId).HasColumnName("backup_storage_info_id");
            Property(x => x.TaskInfoName).HasColumnName("task_info_name");
            Property(x => x.StartDate).HasColumnName("start_date");
            Property(x => x.EndDate).HasColumnName("end_date");
            Property(x => x.Status).HasColumnName("status");
            Property(x => x.StrStatus).HasColumnName("str_status");
            Property(x => x.Type).HasColumnName("task_type");
            Ignore(x => x.BackupStorageInfo);
            Ignore(x => x.StatusInfo);

        }
    }
}
