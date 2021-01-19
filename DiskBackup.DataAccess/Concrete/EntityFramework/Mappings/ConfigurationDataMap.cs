using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class ConfigurationDataMap : EntityTypeConfiguration<ConfigurationData>
    {
        public ConfigurationDataMap()
        {
            ToTable(@"configuration_data");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("configuration_data_id");
            Property(x => x.Key).HasColumnName("key");
            Property(x => x.Value).HasColumnName("value");
        }
    }
}
