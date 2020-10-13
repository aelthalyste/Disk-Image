using DisckBackup.Entities;
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
            Property(x => x.StartDate).HasColumnName("start_date");
            Property(x => x.EndDate).HasColumnName("end_date");
            Property(x => x.BackupType).HasColumnName("backup_type");
            //Property(x => x.TaskName).HasColumnName("task_info_id"); relation
            //Property(x => x.Target).HasColumnName("backup_storage_info_id");
            Property(x => x.Status).HasColumnName("status");
            Property(x => x.StrStatus).HasColumnName("str_status");

        }
    }
}
