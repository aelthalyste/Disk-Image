using DiskBackup.Business.Abstract;
using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class LogService : ILogService
    {
        public bool AddActivityLog(TaskInfo taskInfo)
        {
            throw new NotImplementedException();
        }

        public bool DeleteActivityLog(ActivityLog activityLog)
        {
            throw new NotImplementedException();
        }

        public List<ActivityLog> GetActivityLogList()
        {
            throw new NotImplementedException();
        }

        public List<Log> GetLogList()
        {
            throw new NotImplementedException();
        }
    }
}
