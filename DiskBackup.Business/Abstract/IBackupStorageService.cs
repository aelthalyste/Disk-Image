using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    public interface IBackupStorageService
    {
        List<BackupStorageInfo> BackupStorageInfoList();

        bool AddBackupStorage(BackupStorageInfo backupStorageInfo);
        void DeleteBackupStorage(BackupStorageInfo backupStorageInfo);
        bool UpdateBackupStorage(BackupStorageInfo backupStorageInfo);

    }
}
