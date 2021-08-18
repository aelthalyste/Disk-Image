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
        void SendStatusEMail(TaskInfo taskInfo);
        void SendTestEMail();
        bool SendFeedback(string feedbackMessage, string feedbackType);
    }
}
