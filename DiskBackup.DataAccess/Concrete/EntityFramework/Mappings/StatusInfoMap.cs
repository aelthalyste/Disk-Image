using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class StatusInfoMap : EntityTypeConfiguration<StatusInfo>
    {
        public StatusInfoMap()
        {
            ToTable(@"status_info");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("status_info_id");
            Property(x => x.TaskName).HasColumnName("task_name");
            Property(x => x.FileName).HasColumnName("file_name");
            Property(x => x.TimeElapsed).HasColumnName("time_elapsed");
            Property(x => x.AverageDataRate).HasColumnName("average_data_rate");
            Property(x => x.DataProcessed).HasColumnName("data_processed");
            Property(x => x.InstantDataRate).HasColumnName("instant_data_rate");
            Property(x => x.SourceObje).HasColumnName("source_obje");
            Property(x => x.TotalDataProcessed).HasColumnName("total_data_processed");
            Property(x => x.Status).HasColumnName("status");
            Property(x => x.StrStatus).HasColumnName("str_status");

        }
    }
}
