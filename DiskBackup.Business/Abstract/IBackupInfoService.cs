using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    public interface IBackupInfoService
    {
        List<BackupInfo> BackupInfoList();

        bool AddBackupInfo(BackupInfo backupInfo);
        bool DeleteBackupInfo(BackupInfo backupInfo);
    }
}
