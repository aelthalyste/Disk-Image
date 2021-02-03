using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Data.Entity.ModelConfiguration;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework.Mappings
{
    public class EmailInfoMap : EntityTypeConfiguration<EmailInfo>
    {
        public EmailInfoMap()
        {
            ToTable(@"email_info");
            HasKey(x => x.Id);

            Property(x => x.Id).HasColumnName("email_info_id");
            Property(x => x.EmailAddress).HasColumnName("email_address");
        }
    }
}
