using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class StatusInfo : IEntity
    {
        public int Id { get; set; }
        public string TaskName { get; set; }
        public string FileName { get; set; }
        public long TimeElapsed { get; set; } // backup managerda direkt erişmeye karar verdik
        public double AverageDataRate { get; set; }
        public long DataProcessed { get; set; } //işlenmiş toplam
        public double InstantDataRate { get; set; }
        public string SourceObje { get; set; }
        public long TotalDataProcessed { get; set; } // işlenmesi gereken toplam
        public StatusType Status { get; set; }  // Enum yapıp app.xaml'da gösterim anı için dictionary kullanımından vazgeçilebilir....
        public string StrStatus { get; set; }
    }
}
