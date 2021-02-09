using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    public interface ILicenseService
    {
        void SetDemoFile(string customerName);
        string ValidateLicenseKey(string licenseKey);
        bool ThereIsAFile();
        bool ThereIsARegistryFile();
        string GetRegistryType();
        int GetDemoDaysLeft();
        string[] GetLicenseUserInfo();
        void UpdateDemoLastDate();
        bool IsDemoExpired();
        void FixBrokenRegistry();
        void DeleteRegistryFile();
    }
}
