using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class BackupStorageService : IBackupStorageService
    {
        private IBackupStorageDal _backupStorageDal;

        public BackupStorageService(IBackupStorageDal backupStorageDal)
        {
            _backupStorageDal = backupStorageDal;
        }

        public bool AddBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            var result = _backupStorageDal.Add(backupStorageInfo);
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
            return _backupStorageDal.GetList();
        }

        public void DeleteBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            _backupStorageDal.Delete(backupStorageInfo);
        }

        public bool UpdateBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            var result = _backupStorageDal.Update(backupStorageInfo);
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
