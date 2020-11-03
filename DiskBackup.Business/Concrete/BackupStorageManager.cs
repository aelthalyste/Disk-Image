using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class BackupStorageManager : IBackupStorageService
    {
        private EfBackupStorageDal _BackupStorageDal = new EfBackupStorageDal();

        public bool AddBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            var result = _BackupStorageDal.Add(backupStorageInfo);
            if (result != null)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        public List<BackupStorageInfo> BackupStorageInfoList()
        {
            return _BackupStorageDal.GetList();
        }

        public void DeleteBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            _BackupStorageDal.Delete(backupStorageInfo);
        }

        public bool UpdateBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            var result = _BackupStorageDal.Update(backupStorageInfo);
            if (result != null)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}
