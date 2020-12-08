using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceModel;
using System.ServiceModel.Description;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.TaskScheduler
{
    public class TaskSchedulerClient : ClientBase<ITaskSchedulerManager>
    {
        public TaskSchedulerClient() : base(
            new ServiceEndpoint(
                ContractDescription.GetContract(typeof(ITaskSchedulerManager)),
                new NetNamedPipeBinding() { MaxBufferSize = 500000, MaxBufferPoolSize = 5000000, MaxReceivedMessageSize = 500000, CloseTimeout = TimeSpan.MaxValue, OpenTimeout = TimeSpan.MaxValue, ReceiveTimeout = TimeSpan.MaxValue, SendTimeout = TimeSpan.MaxValue },
                new EndpointAddress("net.pipe://localhost/nardiskbackup/taskscheduler")))
        {

        }

        public ITaskSchedulerManager TaskScheduler { get => Channel; }
    }
}
