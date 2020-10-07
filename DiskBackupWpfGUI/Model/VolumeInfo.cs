using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackupWpfGUI.Model
{
    public class VolumeInfo
    {
        public string VolumeName { get; set; }
        public string Format { get; set; }
        public string VolumeSize { get; set; }
        public string PrioritySection { get; set; }
    }
}
