using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackupWpfGUI.Model
{
    public class DiskInfo
    {
        public string Name { get; set; }
        public string BootType { get; set; }
        public string Size { get; set; }
        public string VolumeName { get; set; }
        public string Format { get; set; }
        public string VolumeSize { get; set; }
    }
}
