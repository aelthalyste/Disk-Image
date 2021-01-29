using LicenseKeyGenerator.Entities;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace LicenseKeyGenerator.DataAccess.Concrete.EntityFramework.Mappings
{
    public class LicenseMap : EntityTypeConfiguration<License>
    {
        public LicenseMap()
        {
            ToTable(@"license_info");
            HasKey(x => x.UniqKey);

            Property(x => x.UniqKey).HasColumnName("uniq_key");
            Property(x => x.DealerName).HasColumnName("dealer_name");
            Property(x => x.CustomerName).HasColumnName("customer_name");
            Property(x => x.AuthorizedPerson).HasColumnName("authorized_person");
            Property(x => x.SupportEndDate).HasColumnName("support_end_date");
            Property(x => x.CreatedDate).HasColumnName("created_date");
            Property(x => x.LicenseVersion).HasColumnName("license_version");
            Property(x => x.Key).HasColumnName("key");
        }

    }
}
