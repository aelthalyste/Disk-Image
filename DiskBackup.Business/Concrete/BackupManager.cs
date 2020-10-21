using DiskBackup.Business.Abstract;
using DiskBackup.Entities.Concrete;
using NarDIWrapper;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.AccessControl;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class BackupManager : IBackupService
    {
                                                                    //TEST EDİLMEDİ

        private DiskTracker _diskTracker = new DiskTracker();

        public bool InitTracker()
        {
            return _diskTracker.CW_InitTracker();
        }

        public List<DiskInformation> GetDiskList()
        {
            //Disk Name, Name (Local Volume vs.), FileSystem (NTFS), FreeSize, PrioritySection, Status
            //Not: Status için eğerki DiskType Raw değilse sağlıklı mı denecekti? Tekrar sor

            //disk'i alıyoruz içine volume dolduruyoruz.
            //ilk önce disk alıp diskin volumlerini bulup doldurmamız gerekir       

            List<DiskInformation> diskList = new List<DiskInformation>();
            VolumeInfo volumeInfo = new VolumeInfo();
            int index = 0;

            foreach (var diskItem in _diskTracker.CW_GetDisksOnSystem())
            {
                diskList[index].DiskId = diskItem.ID;
                diskList[index].Size = diskItem.Size;
                diskList[index].StrSize = FormatBytes(diskItem.Size);

                foreach (var volumeItem in _diskTracker.CW_GetVolumes())
                {
                    volumeInfo.Letter = (char)volumeItem.Letter;
                    volumeInfo.Size = (long)volumeItem.Size;
                    volumeInfo.StrSize = FormatBytes((long)volumeItem.Size);
                    volumeInfo.Bootable = Convert.ToBoolean(volumeItem.Bootable);

                    if (volumeItem.DiskType == 'R')
                    {
                        volumeInfo.DiskType = "RAW";
                        volumeInfo.Status = "Sağlıksız";
                    }
                    else if (volumeItem.DiskType == 'M')
                    {
                        volumeInfo.DiskType = "MBR";
                        volumeInfo.Status = "Sağlıklı";
                    }
                    else if (volumeItem.DiskType == 'G')
                    {
                        volumeInfo.DiskType = "GPT";
                        volumeInfo.Status = "Sağlıklı";
                    }

                    diskList[index].VolumeInfos.Add(volumeInfo);
                }

                index++;
            }

            return diskList;
        }

        public List<BackupInfo> GetBackupFileList(List<BackupStorageInfo> backupStorageList)
        {
            //usedSize, bootable, sıkıştırma, işletim sistemi, pc name, ip address
            List<BackupInfo> backupInfoList = new List<BackupInfo>();
            BackupInfo backupInfo = new BackupInfo();

            foreach (var backupStorageItem in backupStorageList)
            {
                var returnList = _diskTracker.CW_GetBackupsInDirectory(backupStorageItem.Path);
                foreach (var returnItem in returnList)
                {
                    backupInfo.Type = (BackupTypes)returnItem.BackupType; // 2 full - 1 inc - 0 diff - BATU' inc 1 - diff 0
                    backupInfo.Letter = returnItem.Letter;
                    backupInfo.Version = returnItem.Version;
                    backupInfo.OSVolume = returnItem.OSVolume;
                    backupInfo.DiskType = returnItem.DiskType; //mbr gpt

                    backupInfoList.Add(backupInfo);
                }
            }

            return backupInfoList;
        }

        public BackupInfo GetBackupFile(BackupInfo backupInfo) // seçilen dosyanın bilgilerini sağ tarafta kullanabilmek için
        {
            var result = _diskTracker.CW_GetBackupsInDirectory(backupInfo.BackupStorageInfo.Path);

            foreach (var resultItem in result)
            {
                if (resultItem.Version == backupInfo.Version) //içindeki veriler tamamlandığında özel olan veri eşitliği kontrol edilecek
                {
                    //atama işlemleri gerçekleştirilecek
                    backupInfo.OSVolume = resultItem.OSVolume;
                    return backupInfo;
                }
            }

            return null;
        }

        public bool RestoreBackupVolume(BackupInfo backupInfo, VolumeInfo volumeInfo)
        {
            //diskTracker.CW_RestoreToVolume(targetLetter, myBackupMetadata.Letter, myBackupMetadata.Version, true, myMain.myPath);
            return _diskTracker.CW_RestoreToVolume(volumeInfo.Letter, backupInfo.Letter, backupInfo.Version, true, backupInfo.BackupStorageInfo.Path);
        }

        public bool RestoreBackupDisk(BackupInfo backupInfo, DiskInformation diskInformation)
        {
            //diskTracker.CW_RestoreToFreshDisk(targetLetter, myBackupMetadata.Letter, myBackupMetadata.Version, diskID, myMain.myPath);
            //_diskTracker.CW_RestoreToFreshDisk(diskInformation.letter)
            throw new NotImplementedException();
        }

        public bool CreateDifferentialBackup(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            string letters = taskInfo.StrObje;

            int bufferSize = 64 * 1024 * 1024;
            byte[] buffer = new byte[bufferSize];
            StreamInfo str = new StreamInfo();
            long BytesReadSoFar = 0;
            int Read = 0;
            bool result = false;

            foreach (var letter in letters)
            {
                if (_diskTracker.CW_SetupStream(letter, 0, str))
                {
                    unsafe
                    {
                        fixed (byte* BAddr = &buffer[0])
                        {
                            FileStream file = File.Create(backupStorageInfo.Path + str.FileName); //backupStorageInfo path alınıcak
                            while (true)
                            {
                                Read = _diskTracker.CW_ReadStream(BAddr, bufferSize);
                                if (Read == 0)
                                    break;
                                file.Write(buffer, 0, Read);
                                BytesReadSoFar += Read;
                            }
                            result = (long)str.ClusterCount * (long)str.ClusterSize == BytesReadSoFar;
                            _diskTracker.CW_TerminateBackup(result); //işlemi başarılı olup olmadığı

                            try
                            {
                                File.Copy(str.MetadataFileName, backupStorageInfo.Path + str.MetadataFileName); //backupStorageInfo path alınıcak
                            }
                            catch (IOException iox)
                            {
                                //MessageBox.Show(iox.Message);
                            }

                            if (result == true)
                            {
                                _diskTracker.CW_SaveBootState();
                            }
                            file.Close();
                        }
                    }
                }
            }
            return result;
        }

        public bool CreateIncrementalBackup(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            string letters = taskInfo.StrObje;

            int bufferSize = 64 * 1024 * 1024;
            byte[] buffer = new byte[bufferSize];
            StreamInfo str = new StreamInfo();
            long BytesReadSoFar = 0;
            int Read = 0;
            bool result = false;

            foreach (var letter in letters)
            {
                if (_diskTracker.CW_SetupStream(letter, 1, str))
                {
                    unsafe
                    {
                        fixed (byte* BAddr = &buffer[0])
                        {
                            FileStream file = File.Create(backupStorageInfo.Path + str.FileName); //backupStorageInfo path alınıcak
                            while (true)
                            {
                                Read = _diskTracker.CW_ReadStream(BAddr, bufferSize);
                                if (Read == 0)
                                    break;
                                file.Write(buffer, 0, Read);
                                BytesReadSoFar += Read;
                            }
                            result = (long)str.ClusterCount * (long)str.ClusterSize == BytesReadSoFar;
                            _diskTracker.CW_TerminateBackup(result); //işlemin başarılı olup olmadığı

                            try
                            {
                                File.Copy(str.MetadataFileName, backupStorageInfo.Path + str.MetadataFileName); //backupStorageInfo path alınıcak
                            }
                            catch (IOException iox)
                            {
                                //MessageBox.Show(iox.Message);
                            }

                            if (result == true)
                            {
                                _diskTracker.CW_SaveBootState();
                            }
                            file.Close();
                        }
                    }
                }
            }
            return result;
        }

        public bool CreateFullBackup(TaskInfo taskInfo) //bu method daha gelmedi 
        {
            throw new NotImplementedException();
        }

        public List<FilesInBackup> GetFileInfoList() //bu method daha gelmedi
        {
            throw new NotImplementedException();
        }

        public List<Log> GetLogList() //bu method daha gelmedi
        {
            throw new NotImplementedException();
        }

        public bool PauseTask(TaskInfo taskInfo) //bu method tekrar konuşulacak
        {
            if (taskInfo.Type == 0) //backup
            {
                
            }
            else //restore
            {

            }
            throw new NotImplementedException();
        }

        public bool CancelTask(TaskInfo taskInfo) 
        {
            if (taskInfo.Type == 0) //backup
            {
                _diskTracker.CW_TerminateBackup(false);  //_diskTracker bunun hangi task olduğunu nasıl anlayacak ??
            }
            else //restore
            {

            }
            throw new NotImplementedException();
        }

        public bool ResumeTask(TaskInfo taskInfo)  //bu method tekrar konuşulacak
        {
            if (taskInfo.Type == 0) //backup
            {

            }
            else //restore
            {

            }
            throw new NotImplementedException();
        }

        public bool RestoreFile(BackupInfo backupInfo, FilesInBackup fileInfo, string destination) //bu method daha gelmedi
        {
            throw new NotImplementedException();
        }

        public bool RestoreFolder(BackupInfo backupInfo, FilesInBackup fileInfo, string destination) //bu method daha gelmedi
        {
            throw new NotImplementedException();
        }

        private string FormatBytes(long bytes)
        {
            string[] Suffix = { "B", "KB", "MB", "GB", "TB" };
            int i;
            double dblSByte = bytes;
            for (i = 0; i < Suffix.Length && bytes >= 1024; i++, bytes /= 1024)
            {
                dblSByte = bytes / 1024.0;
            }

            return String.Format("{0:0.##} {1}", dblSByte, Suffix[i]);
        }
    }
}
