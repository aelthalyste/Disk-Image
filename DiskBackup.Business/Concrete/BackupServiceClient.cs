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
    public class BackupServiceClient : ClientBase<IBackupService>
    {
        public BackupServiceClient() : base(
            new ServiceEndpoint(
                ContractDescription.GetContract(typeof(IBackupService)),
                new NetNamedPipeBinding() { MaxBufferSize = 500000, MaxBufferPoolSize = 5000000, MaxReceivedMessageSize = 500000 , CloseTimeout = TimeSpan.MaxValue, OpenTimeout= TimeSpan.MaxValue, ReceiveTimeout = TimeSpan.MaxValue, SendTimeout = TimeSpan.MaxValue },
                new EndpointAddress("net.pipe://localhost/nardiskbackup/backupservice")))
        {

        }
        public IBackupService BackupService { get => Channel; }
    }
}
