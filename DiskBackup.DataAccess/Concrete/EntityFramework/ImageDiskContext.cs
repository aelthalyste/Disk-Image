using DiskBackup.Entities.Concrete;
using DiskBackup.DataAccess.Concrete.EntityFramework.Mappings;
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
        public DbSet<BackupStorageInfo> BackupStorageInfos { get; set; }
        public DbSet<BackupTask> BackupTasks { get; set; }
        public DbSet<RestoreTask> RestoreTasks { get; set; }
        public DbSet<TaskInfo> TaskInfos { get; set; }
        public DbSet<StatusInfo> StatusInfos { get; set; }

        protected override void OnModelCreating(DbModelBuilder modelBuilder)
        {
            modelBuilder.Configurations.Add(new ActivitiyLogMap());
            modelBuilder.Configurations.Add(new BackupStorageInfoMap());
            modelBuilder.Configurations.Add(new BackupTaskMap());
            modelBuilder.Configurations.Add(new RestoreTaskMap());
            modelBuilder.Configurations.Add(new TaskInfoMap());
            modelBuilder.Configurations.Add(new StatusInfoMap());
        }
    }
}
