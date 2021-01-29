using LicenseKeyGenerator.DataAccess.Concrete.EntityFramework.Mappings;
using LicenseKeyGenerator.Entities;
using System;
using System.Collections.Generic;
using System.Data.Entity;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace LicenseKeyGenerator.DataAccess.Concrete.EntityFramework
{
    public class KeyGenaratorContext : DbContext
    {
        public KeyGenaratorContext()
        {
            Database.SetInitializer<KeyGenaratorContext>(null);
        }
        
        public DbSet<License> Licenses { get; set; }

        protected override void OnModelCreating(DbModelBuilder modelBuilder)
        {
            modelBuilder.Configurations.Add(new LicenseMap());
        }
    }
}
