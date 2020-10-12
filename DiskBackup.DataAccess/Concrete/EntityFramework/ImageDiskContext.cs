using DisckBackup.Entities;
using System;
using System.Collections.Generic;
using System.Data.Entity;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework
{
    public class ImageDiskContext : DbContext
    {
        public ImageDiskContext()
        {
            Database.SetInitializer<ImageDiskContext>(null);
        }
        public DbSet<ActivityLog> ActivityLogs { get; set; }
    }
}
