using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.Entities.Concrete;
using Serilog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    [ServiceBehavior(InstanceContextMode = InstanceContextMode.Single)]
    public class BackupStorageService : IBackupStorageService
    {
        private IBackupStorageDal _backupStorageDal;
        private readonly ILogger _logger;


        public BackupStorageService(IBackupStorageDal backupStorageDal, ILogger logger) 
        {
            _backupStorageDal = backupStorageDal;
            _logger = logger.ForContext<BackupStorageInfo>();
        }

        public bool AddBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            _logger.Verbose("AddBackupStorage çağırıldı");

            //if(backupStorageInfo.Type == BackupStorageType.NAS && !ValideNasConnection(backupStorageInfo.Path, backupStorageInfo.Username, backupStorageInfo.Password, backupStorageInfo.Domain))
            //{
            //    throw new Exception("NAS storage validation has failed. Check your network and credentials.");
            //}

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
            _logger.Verbose("BackupStorageInfoList çağırıldı");
            return _backupStorageDal.GetList();
        }

        public void DeleteBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            _logger.Verbose("DeleteBackupStorage çağırıldı");
            _backupStorageDal.Delete(backupStorageInfo);
        }

        public bool UpdateBackupStorage(BackupStorageInfo backupStorageInfo)
        {
            _logger.Verbose("UpdateBackupStorage çağırıldı");

            /*if (backupStorageInfo.Type == BackupStorageType.NAS && !ValideNasConnection(backupStorageInfo.Path, backupStorageInfo.Username, backupStorageInfo.Password, backupStorageInfo.Domain))
            {
                throw new Exception("NAS storage validation has failed. Check your network and credentials.");
            }*/

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

        public bool ValidateNasConnection(string nasAddr, string userName, string password, string domain)
        {
            _logger.Verbose("ValidateNasConnection çağırıldı");

            try
            {
                using (new NetworkConnection(nasAddr, new System.Net.NetworkCredential(userName, password, domain)))
                {
                    var dirInfo = new DirectoryInfo(nasAddr);
                    return dirInfo.Exists;
                }

            }
            catch (Exception)
            {

            }
            return false;
        }
    }
}
