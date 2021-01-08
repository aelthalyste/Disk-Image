using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackupWpfGUI.Utils
{
    public interface IConfigHelper
    {
        string GetConfig(string key);
        void SetConfig(string key, string value);
    }
}
