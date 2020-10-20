using DiskBackup.Business.Abstract;
using DiskBackup.Entities.Concrete;
using NarDIWrapper;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class BackupManager : IBackupService
    {
        private DiskTracker _diskTracker;

        public bool CancelTask(TaskInfo taskInfo)
        {
            throw new NotImplementedException();
        }

        public bool CreateDifferentialBackup(VolumeInfo volumeInfo)
        {
            throw new NotImplementedException();
        }

        public bool CreateFullBackup(VolumeInfo volumeInfo)
        {
            throw new NotImplementedException();
        }

        public bool CreateIncrementalBackup(VolumeInfo volumeInfo)
        {
            throw new NotImplementedException();
        }

        public List<RestoreTask> GetBackupFileList()
        {
            //diskTracker.CW_GetBackupsInDirectory(myMain.myPath)
            throw new NotImplementedException();
        }

        public List<Entities.Concrete.DiskInfo> GetDiskList()
        {
            var aa = _diskTracker.CW_GetVolumes();
            
            throw new NotImplementedException();
        }

        public List<FileInfo> GetFileInfoList()
        {
            throw new NotImplementedException();
        }

        public List<Log> GetLogList()
        {
            throw new NotImplementedException();
        }

        public bool InitTracker()
        {
            return _diskTracker.CW_InitTracker();
        }

        public bool PauseTask(TaskInfo taskInfo)
        {
            throw new NotImplementedException();
        }

        public bool RestoreBackup(BackupInfo backupInfo, VolumeInfo volumeInfo)
        {
            throw new NotImplementedException();
        }

        public bool RestoreFile(BackupInfo backupInfo, FileInfo fileInfo, string destination)
        {
            throw new NotImplementedException();
        }

        public bool RestoreFolder(BackupInfo backupInfo, FileInfo fileInfo, string destination)
        {
            throw new NotImplementedException();
        }

        public bool ResumeTask(TaskInfo taskInfo)
        {
            throw new NotImplementedException();
        }

        List<Entities.Concrete.DiskInfo> IBackupService.GetDiskList()
        {
            throw new NotImplementedException();
        }
    }
}
