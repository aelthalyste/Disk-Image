using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    [ServiceContract]
    public interface IBackupStorageService
    {
        [OperationContract]
        List<BackupStorageInfo> BackupStorageInfoList();

        [OperationContract]
        bool AddBackupStorage(BackupStorageInfo backupStorageInfo);
        [OperationContract]
        void DeleteBackupStorage(BackupStorageInfo backupStorageInfo);
        [OperationContract]
        bool UpdateBackupStorage(BackupStorageInfo backupStorageInfo);
        [OperationContract]
        bool ValidateNasConnection(string nasAddr, string userName, string password, string domain);
    }
}
