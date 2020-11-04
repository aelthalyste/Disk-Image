using DiskBackup.Business.Abstract;
using DiskBackup.Entities.Concrete;
using NarDIWrapper;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.AccessControl;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class BackupManager : IBackupService
    {
        private DiskTracker _diskTracker = new DiskTracker();
        private CSNarFileExplorer _cSNarFileExplorer = new CSNarFileExplorer();

        private ManualResetEvent _manualResetEvent = new ManualResetEvent(true);
        private CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();
        private bool _isStarted = false;

        private Stopwatch _timeElapsed = new Stopwatch();
        private StatusInfo _statusInfo = new StatusInfo();

        public bool InitTracker()
        {
            // programla başlat 1 kere çalışması yeter
            // false değeri dönüyor ise eğer backup işlemlerini disable et
            return _diskTracker.CW_InitTracker();
        }

        public void InitFileExplorer(BackupInfo backupInfo) //initTracker'la aynı mantıkla çalışır mı? (Explorer ctor'da 1 kere çağrılma)
        {
            //return _cSNarFileExplorer.CW_Init(12,backupInfo.Letter,backupInfo.Version,(char)backupInfo.BackupStorageInfo.Path); 
            //rootDir char biz buraya ne dönücez
            //Handle options şimdilik ne verilecek
            _cSNarFileExplorer.CW_Init('C', 0, "");

        }

        public List<DiskInformation> GetDiskList()
        {
            //Disk Name, Name (Local Volume vs.), FileSystem (NTFS), FreeSize, PrioritySection, Status 
            List<DiskInfo> disks = DiskTracker.CW_GetDisksOnSystem();
            List<VolumeInformation> volumes = DiskTracker.CW_GetVolumes();

            List<DiskInformation> diskList = new List<DiskInformation>();
            int index = 0;

            foreach (DiskInfo diskItem in disks)
            {
                DiskInformation temp = new DiskInformation();
                temp.DiskId = diskItem.ID;
                temp.Size = (long)diskItem.Size;
                temp.StrSize = FormatBytes((long)diskItem.Size);
                diskList.Add(temp);

                foreach (var volumeItem in volumes)
                {
                    if (volumeItem.DiskID == temp.DiskId)
                    {
                        VolumeInfo volumeInfo = new VolumeInfo();
                        volumeInfo.Letter = (char)volumeItem.Letter;
                        volumeInfo.Size = (long)volumeItem.TotalSize;
                        volumeInfo.StrSize = FormatBytes((long)volumeItem.TotalSize);
                        volumeInfo.FreeSize = (long)volumeItem.FreeSize;
                        volumeInfo.StrFreeSize = FormatBytes((long)volumeItem.FreeSize);
                        volumeInfo.Bootable = Convert.ToBoolean(volumeItem.Bootable);
                        volumeInfo.Name = volumeItem.VolumeName;
                        volumeInfo.DiskName = "Disk " + temp.DiskId;
                        volumeInfo.FileSystem = "NTFS";

                        // volumeItem.Bootable true ise işletim sistemi var 
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
                }
                index++;
            }
            return diskList;
        }

        public List<BackupInfo> GetBackupFileList(List<BackupStorageInfo> backupStorageList)
        {
            //usedSize, bootable, sıkıştırma, pc name, ip address
            List<BackupInfo> backupInfoList = new List<BackupInfo>();
            BackupInfo backupInfo = new BackupInfo();
            //bootable = osVolume (true)
            foreach (var backupStorageItem in backupStorageList)
            {
                var returnList = DiskTracker.CW_GetBackupsInDirectory(backupStorageItem.Path);
                foreach (var returnItem in returnList)
                {
                    backupInfo.Letter = returnItem.Letter;
                    backupInfo.Version = returnItem.Version;
                    backupInfo.OSVolume = returnItem.OSVolume;
                    backupInfo.DiskType = returnItem.DiskType; //mbr gpt
                    backupInfo.OS = returnItem.WindowsName;

                    if (returnItem.Version == -1)
                        backupInfo.Type = BackupTypes.Full;
                    else
                        backupInfo.Type = (BackupTypes)returnItem.BackupType; // 2 full - 1 inc - 0 diff - BATU' inc 1 - diff 0

                    backupInfoList.Add(backupInfo);
                }
            }

            return backupInfoList;
        }

        public BackupInfo GetBackupFile(BackupInfo backupInfo)
        {
            // seçilen dosyanın bilgilerini sağ tarafta kullanabilmek için
            var result = DiskTracker.CW_GetBackupsInDirectory(backupInfo.BackupStorageInfo.Path);

            foreach (var resultItem in result)
            {
                if (resultItem.Version == backupInfo.Version && resultItem.BackupType.Equals(backupInfo.Type) && resultItem.DiskType.Equals(backupInfo.DiskType))
                {
                    backupInfo.Letter = resultItem.Letter;
                    backupInfo.Version = resultItem.Version;
                    backupInfo.OSVolume = resultItem.OSVolume;
                    backupInfo.DiskType = resultItem.DiskType; //mbr gpt
                    backupInfo.OS = resultItem.WindowsName;

                    if (resultItem.Version == -1)
                        backupInfo.Type = BackupTypes.Full;
                    else
                        backupInfo.Type = (BackupTypes)resultItem.BackupType; // 2 full - 1 inc - 0 diff - BATU' inc 1 - diff 0
                    return backupInfo;
                }
            }

            return null;
        }

        public bool RestoreBackupVolume(BackupInfo backupInfo, char volumeLetter)
        {
            return DiskTracker.CW_RestoreToVolume(volumeLetter, backupInfo.Letter, backupInfo.Version, true, backupInfo.BackupStorageInfo.Path); //true gidecek
            throw new NotImplementedException();
        }

        public bool RestoreBackupDisk(BackupInfo backupInfo, DiskInformation diskInformation)
        {
            //diskTracker.CW_RestoreToFreshDisk(targetLetter, myBackupMetadata.Letter, myBackupMetadata.Version, diskID, myMain.myPath);
            //pathde sadece path varmış dosya adı yokmuş
            //target letter nerden
            //batudan fonksiyon gelecek o fonksiyon hangi harfle restore edeceğini dönecek ve batu o harfle restore edecek
            //CW_GetFirstAvailableVolumeLetter ile boş olan volume'ü alıp batuhan'a dönerek o volume restore gerçekleştirilecek
            throw new NotImplementedException();
        }

        public bool CreateIncDiffBackup(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo)
        {
            _timeElapsed.Start();

            _isStarted = true;
            var cancellationToken = _cancellationTokenSource.Token;

            string letters = taskInfo.StrObje;

            int bufferSize = 64 * 1024 * 1024;
            byte[] buffer = new byte[bufferSize];
            StreamInfo str = new StreamInfo();
            long BytesReadSoFar = 0;
            int Read = 0;
            bool result = false;

            _statusInfo.TaskName = taskInfo.Name;
            _statusInfo.FileName = backupStorageInfo.Path + "/" + str.MetadataFileName;
            _statusInfo.SourceObje = taskInfo.StrObje;

            Task.Run(() =>
            {
                foreach (var letter in letters) // C D E F
                {
                    if (_diskTracker.CW_SetupStream(letter, (int)taskInfo.BackupTaskInfo.Type, str)) // 0 diff, 1 inc, full (2) ucu gelmediğinden ayrılabilir veya aynı devam edebilir
                    {
                        unsafe
                        {
                            fixed (byte* BAddr = &buffer[0])
                            {
                                FileStream file = File.Create(backupStorageInfo.Path + str.FileName); //backupStorageInfo path alınıcak
                                while (true)
                                {
                                    if (cancellationToken.IsCancellationRequested)
                                    {
                                        //cleanup 
                                        _diskTracker.CW_TerminateBackup(false, letter);
                                        return;
                                    }
                                    _manualResetEvent.WaitOne();

                                    Read = _diskTracker.CW_ReadStream(BAddr, letter, bufferSize);
                                    file.Write(buffer, 0, Read);
                                    BytesReadSoFar += Read;


                                    _statusInfo.DataProcessed = BytesReadSoFar;
                                    _statusInfo.TotalDataProcessed = (long)str.CopySize;
                                    _statusInfo.AverageDataRate = ((_statusInfo.TotalDataProcessed / 1024.0) / 1024.0) / (_timeElapsed.ElapsedMilliseconds / 1000); // MB/s
                                    _statusInfo.InstantDataRate = ((BytesReadSoFar / 1024.0) / 1024.0) / (_timeElapsed.ElapsedMilliseconds / 1000); // MB/s

                                    if (Read != bufferSize)
                                        break;
                                }
                                result = (long)str.CopySize == BytesReadSoFar;
                                _diskTracker.CW_TerminateBackup(result, letter); //işlemi başarılı olup olmadığı cancel gelmeden

                                try
                                {
                                    File.Copy(str.MetadataFileName, backupStorageInfo.Path + str.MetadataFileName); //backupStorageInfo path alınıcak
                                    //backupStorageInfo.Path ters slaş '\' ile bitmeli
                                }
                                catch (IOException iox)
                                {
                                    //MessageBox.Show(iox.Message);
                                }
                                file.Close();
                            }
                        }
                    }
                }
            });

            return result;
        }

        public bool CreateFullBackup(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo) //bu method daha gelmedi 
        {
            throw new NotImplementedException();
        }

        public void PauseTask(TaskInfo taskInfo)
        {
            if (!_isStarted) throw new Exception("Backup is not started");
            _timeElapsed.Stop();
            _manualResetEvent.Reset();
        }

        public void CancelTask(TaskInfo taskInfo)
        {
            _cancellationTokenSource.Cancel();
            _isStarted = false;
            _timeElapsed.Stop();
            _timeElapsed.Reset();
            _manualResetEvent.Set();
        }

        public void ResumeTask(TaskInfo taskInfo)
        {
            if (!_isStarted) throw new Exception("Backup is not started");
            _timeElapsed.Start();
            _manualResetEvent.Set();
        }

        public List<FilesInBackup> GetFileInfoList() //EKSİKLERİ VAR
        {
            var resultList = _cSNarFileExplorer.CW_GetFilesInCurrentDirectory();
            List<FilesInBackup> filesInBackupList = new List<FilesInBackup>();
            foreach (var item in resultList)
            {
                filesInBackupList.Add(new FilesInBackup
                {
                    Name = item.Name,
                    Type = (FileType)Convert.ToInt16(item.IsDirectory), //Directory ise 1 
                    Size = (long)item.Size,
                    StrSize = FormatBytes((long)item.Size),
                    UpdatedDate = Convert.ToInt32(item.LastModifiedTime.Day) + "." +
                        Convert.ToInt32(item.LastModifiedTime.Month) + "." +
                        Convert.ToInt32(item.LastModifiedTime.Year) + " " +
                        Convert.ToInt32(item.LastModifiedTime.Hour) + ":" +
                        Convert.ToInt32(item.LastModifiedTime.Minute),
                    //Size = (long)item.Size,
                    // StrSize = FormatBytes((long)item.Size),
                    Id = (long)item.ID,
                    //StrSize = 
                    //    item.LastModifiedTime.Day.ToString() + "." + 
                    //    item.LastModifiedTime.Month.ToString() + "." +
                    //    item.LastModifiedTime.Year.ToString() + " " +
                    //    item.LastModifiedTime.Hour.ToString() + ":" +
                    //    item.LastModifiedTime.Minute.ToString()
                    //Path değeri Batudan isteyelim
                    //UpdatedDate dönüşü daha yok
                });
            }
            return filesInBackupList;
        }

        public void FreeFileExplorer()
        {
            _cSNarFileExplorer.CW_Free();
        }

        public bool GetSelectedFileInfo(FilesInBackup filesInBackup)
        {
            CSNarFileEntry cSNarFileEntry = new CSNarFileEntry();
            cSNarFileEntry.ID = (ulong)filesInBackup.Id;
            cSNarFileEntry.IsDirectory = Convert.ToBoolean(filesInBackup.Type); //bool demişti short dönüyor? 1-0 hangisi file hangisi folder
            cSNarFileEntry.Name = filesInBackup.Name;
            cSNarFileEntry.Size = (ulong)filesInBackup.Size;
            //tarihler eklenecek. oluşturma tarihi önemli mi?
            return _cSNarFileExplorer.CW_SelectDirectory((ulong)cSNarFileEntry.ID);
        }

        public void PopDirectory()
        {
            _cSNarFileExplorer.CW_PopDirectory();
        }

        public string GetCurrentDirectory()
        {
            return _cSNarFileExplorer.CW_GetCurrentDirectoryString();
        }

        public List<Log> GetLogList() //bu method daha gelmedi
        {
            throw new NotImplementedException();
        }

        public bool RestoreFilesInBackup(BackupInfo backupInfo, FilesInBackup fileInfo, string destination) //bu method daha gelmedi
        {
            //_cSNarFileExplorer.CW_RestoreFile(ID, seçilen backup'ın directorysi dosya ismi olmadan, yüklenecek yer)
            //void CW_RestoreFile(INT64 ID);
            //_cSNarFileExplorer.CW_Free FileExplorer kapatıldığında Free çağırmakta fayda var bellek yönetimi için tutulan alanları geri veriyor sisteme
            throw new NotImplementedException();
        }

        public StatusInfo GetStatusInfo()
        {
            _statusInfo.TimeElapsed = _timeElapsed.ElapsedMilliseconds; // TimeSpan.FromMilliseconds(999999); Console.WriteLine($"{d:mm\\:ss}");
            return _statusInfo;
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

            if (i > 4)
                i = 4;

            return ($"{dblSByte:0.##} {Suffix[i]}");
        }

    }
}
