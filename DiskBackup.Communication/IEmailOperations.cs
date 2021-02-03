using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Communication
{
    public interface IEmailOperations
    {

        void SendMail(List<EmailInfo> emailAddresses, StatusInfo statusInfo);

        string ChangeBody(string lang, StatusInfo statusInfo);

        string ChangeLang(string lang);
    }
}
