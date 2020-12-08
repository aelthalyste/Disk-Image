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
    public class LogServiceClient : ClientBase<ILogService>
    {
        public LogServiceClient() : base(
            new ServiceEndpoint(
                ContractDescription.GetContract(typeof(ILogService)),
                new NetNamedPipeBinding() { MaxBufferSize = 500000, MaxBufferPoolSize = 5000000, MaxReceivedMessageSize = 500000, CloseTimeout = TimeSpan.MaxValue, OpenTimeout = TimeSpan.MaxValue, ReceiveTimeout = TimeSpan.MaxValue, SendTimeout = TimeSpan.MaxValue },
                new EndpointAddress("net.pipe://localhost/nardiskbackup/logservice")))
        {

        }
        public ILogService LogService { get => Channel; }
    }
}
