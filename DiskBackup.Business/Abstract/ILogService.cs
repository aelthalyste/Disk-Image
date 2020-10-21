using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    public interface ILogService
    {
        List<ActivityLog> GetActivityLogList();
        List<Log> GetLogList();

        bool DeleteActivityLog(ActivityLog activityLog);
        bool AddActivityLog(TaskInfo taskInfo);

        //Log'u yazdıracağımız txt dosyası için okuma ve yazma metodları eklenebilir.... 
    }
}
