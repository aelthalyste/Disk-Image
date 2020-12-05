using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class ActivityDownLog
    {
        //Geçmiş aktiviteler tabında listviewLogDown da gözükecek
        public string Time { get; set; }
        public string Detail { get; set; }
    }
}
