using DiskBackup.Entities;
using DiskBackup.DataAccess.Core;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Abstract
{
    public interface IBackupTaskDal : IEntityRepository<BackupTask>
    {
    }
}
