using DiskBackup.Entities.Concrete;
using System.Collections.Generic;
using System.ServiceModel;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    [ServiceContract]
    public interface ILogService
    {
        [OperationContract]
        List<ActivityLog> GetActivityLogList();
        [OperationContract]
        List<ActivityDownLog> GetLogList();

        [OperationContract]
        bool DeleteActivityLog(ActivityLog activityLog);
        [OperationContract]
        bool AddActivityLog(TaskInfo taskInfo);

        //Log'u yazdıracağımız txt dosyası için okuma ve yazma metodları eklenebilir.... 
    }
}
