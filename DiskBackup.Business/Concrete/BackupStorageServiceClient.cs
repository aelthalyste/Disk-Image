using DiskBackup.Business.Abstract;
using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.ServiceModel.Description;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class BackupStorageServiceClient : ClientBase<IBackupStorageService>
    {
        public BackupStorageServiceClient() : base(
            new ServiceEndpoint(
                ContractDescription.GetContract(typeof(IBackupStorageService)),
                new NetNamedPipeBinding(),
                new EndpointAddress("net.pipe://localhost/nardiskbackup/backupstorageservice")))
        {

        }
        public IBackupStorageService BackupStorageService { get => Channel; }
    }
}
