using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Communication
{
    public interface IEMailOperations
    {
        void SendEMail(TaskInfo taskInfo);
        void SendTestEMail();
    }
}
