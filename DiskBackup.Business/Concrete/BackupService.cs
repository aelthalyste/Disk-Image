﻿using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.DataAccess.Core;
using DiskBackup.Entities.Concrete;
using NarDIWrapper;
using Serilog;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.AccessControl;
using System.ServiceModel;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    [ServiceBehavior(InstanceContextMode = InstanceContextMode.Single)]
    public class BackupService : IBackupService
    {
        private DiskTracker _diskTracker = new DiskTracker();
        private CSNarFileExplorer _cSNarFileExplorer = new CSNarFileExplorer();

        private Dictionary<int, ManualResetEvent> _taskEventMap = new Dictionary<int, ManualResetEvent>(); // aynı işlem Stopwatch ve cancellationtoken source için de yapılacak ancak Quartz'ın pause, resume ve cancel işlemleri düzgün çalışıyorsa kullanılmayacak
        private Dictionary<int, CancellationTokenSource> _cancellationTokenSource = new Dictionary<int, CancellationTokenSource>();
        private Dictionary<int, Stopwatch> _timeElapsedMap = new Dictionary<int, Stopwatch>();

        private bool _isStarted = false;
        private bool _initTrackerResult = false;
        private bool _refreshIncDiffTaskFlag = false;
        private bool _refreshIncDiffLogFlag = false; // Refreshde yenilememe durumu olması durumunda her task için ayrı flagler oluşturulacak

        private readonly IStatusInfoDal _statusInfoDal; // status bilgilerini veritabanına yazabilmek için gerekli
        private readonly ITaskInfoDal _taskInfoDal; // status bilgilerini veritabanına yazabilmek için gerekli
        private readonly ILogger _logger;

        public BackupService(IStatusInfoDal statusInfoRepository, ITaskInfoDal taskInfoDal, ILogger logger)
        {
            _statusInfoDal = statusInfoRepository;
            _taskInfoDal = taskInfoDal;
            _logger = logger.ForContext<BackupService>();
        }

        public bool InitTracker()
        {
            // programla başlat 1 kere çalışması yeter
            // false değeri dönüyor ise eğer backup işlemlerini disable et
            _logger.Verbose("InitTracker metodu çağırıldı");
            if (!_isStarted)
            {
                _initTrackerResult = _diskTracker.CW_InitTracker();
                _isStarted = true;
            }
            return _initTrackerResult;
        }

        public bool GetInitTracker()
        {
            _logger.Verbose("GetInitTracker metodu çağırıldı");
            return _initTrackerResult;
        }

        public bool GetRefreshIncDiffTaskFlag()
        {
            _logger.Verbose("GetRefreshIncDiffTaskFlag metodu çağırıldı");
            return _refreshIncDiffTaskFlag;
        }

        public void RefreshIncDiffTaskFlag(bool value)
        {
            _logger.Verbose("RefreshIncDiffTaskFlag metodu çağırıldı");
            _refreshIncDiffTaskFlag = value;
        }

        public bool GetRefreshIncDiffLogFlag()
        {
            _logger.Verbose("GetRefreshIncDiffLogFlag metodu çağırıldı");
            return _refreshIncDiffLogFlag;
        }

        public void RefreshIncDiffLogFlag(bool value)
        {
            _logger.Verbose("RefreshIncDiffLogFlag metodu çağırıldı");
            _refreshIncDiffLogFlag = value;
        }

        public void InitFileExplorer(BackupInfo backupInfo) //initTracker'la aynı mantıkla çalışır mı? (Explorer ctor'da 1 kere çağrılma)
        {
            _logger.Verbose("InitFileExplorer metodu çağırıldı");
            //rootDir string biz buraya ne dönücez
            // yedek volumu, versiondan gelecek, "E:\ebru-eyupDeneme"-- ters slaş ekle sonuna
            //_cSNarFileExplorer.CW_Init('C', 0, "");
            //_cSNarFileExplorer.CW_Init(backupInfo.Letter, backupInfo.Version, backupInfo.BackupStorageInfo.Path);
            _logger.Verbose("İnitFileExplorer: {path}", backupInfo.BackupStorageInfo.Path + backupInfo.FileName);
            _cSNarFileExplorer.CW_Init(backupInfo.BackupStorageInfo.Path + backupInfo.FileName); // isim eklenmesi gerekmeli gibi
        }

        public List<DiskInformation> GetDiskList()
        {
            _logger.Verbose("GetDiskList metodu çağırıldı");
            //PrioritySection 
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
            _logger.Verbose("GetBackupFileList metodu çağırıldı");
            List<BackupInfo> backupInfoList = new List<BackupInfo>();
            _logger.Verbose("1");
            //bootable = osVolume (true)
            foreach (BackupStorageInfo backupStorageItem in backupStorageList)
            {
                var returnList = DiskTracker.CW_GetBackupsInDirectory(backupStorageItem.Path);
                _logger.Verbose("2");

                foreach (var returnItem in returnList)
                {
                    BackupInfo backupInfo = new BackupInfo();
                    _logger.Verbose("3");
                    backupInfo.Letter = returnItem.Letter;
                    _logger.Verbose("4");

                    backupInfo.Version = returnItem.Version;
                    _logger.Verbose("5");

                    backupInfo.OSVolume = returnItem.OSVolume;
                _logger.Verbose("6");
                    backupInfo.DiskType = returnItem.DiskType; //mbr gpt
                _logger.Verbose("7");
                    backupInfo.OS = returnItem.WindowsName;
                _logger.Verbose("8");
                    backupInfo.BackupTaskName = returnItem.TaskName;
                _logger.Verbose("9");
                    backupInfo.Description = returnItem.TaskDescription;
                _logger.Verbose("10");
                    backupInfo.BackupStorageInfo = backupStorageItem;
                _logger.Verbose("11");
                    backupInfo.BackupStorageInfoId = backupStorageItem.Id;

                _logger.Verbose("12");
                    backupInfo.Bootable = Convert.ToBoolean(returnItem.OSVolume);
                _logger.Verbose("13");
                    backupInfo.VolumeSize = (long)returnItem.VolumeTotalSize;
                _logger.Verbose("14");
                    backupInfo.StrVolumeSize = FormatBytes((long)returnItem.VolumeTotalSize);
                _logger.Verbose("15");
                    backupInfo.UsedSize = (long)returnItem.VolumeUsedSize;
                _logger.Verbose("16");
                    backupInfo.StrUsedSize = FormatBytes((long)returnItem.VolumeUsedSize);
                _logger.Verbose("17");
                    backupInfo.FileSize = (long)returnItem.BytesNeedToCopy;
                _logger.Verbose("18");
                    backupInfo.StrFileSize = FormatBytes((long)returnItem.BytesNeedToCopy);
                _logger.Verbose("19");
                    backupInfo.FileName = returnItem.Fullpath.Split('\\').Last();
                _logger.Verbose("20");
                    backupInfo.PCName = returnItem.ComputerName;
                _logger.Verbose("21");
                    backupInfo.IpAddress = returnItem.IpAdress;
                _logger.Verbose("22");
                    string createdDate = (returnItem.BackupDate.Day < 10) ? 0 + returnItem.BackupDate.Day.ToString() : returnItem.BackupDate.Day.ToString();
                _logger.Verbose("23");
                    createdDate = createdDate + "." + ((returnItem.BackupDate.Month < 10) ? 0 + returnItem.BackupDate.Month.ToString() : returnItem.BackupDate.Month.ToString());
                _logger.Verbose("24");
                    createdDate = createdDate + "." + returnItem.BackupDate.Year + " ";
                _logger.Verbose("25");
                    createdDate = createdDate + ((returnItem.BackupDate.Hour < 10) ? 0 + returnItem.BackupDate.Hour.ToString() : returnItem.BackupDate.Hour.ToString());
                _logger.Verbose("26");
                    createdDate = createdDate + ":" + ((returnItem.BackupDate.Minute < 10) ? 0 + returnItem.BackupDate.Minute.ToString() : returnItem.BackupDate.Minute.ToString());
                _logger.Verbose("27");
                    createdDate = createdDate + ":" + returnItem.BackupDate.Second.ToString();
                _logger.Verbose("28: -{tarih}-", createdDate);
                    backupInfo.CreatedDate = createdDate;
                    /*try
                    {
                        backupInfo.CreatedDate = Convert.ToDateTime(createdDate.ToString());

                    }
                    catch(Exception ex)
                    {
                        _logger.Error(ex, "hatamız hazır");
                    }*/

                _logger.Verbose("29");
                    if (returnItem.Version == -1)
                        backupInfo.Type = BackupTypes.Full;
                    else
                        backupInfo.Type = (BackupTypes)returnItem.BackupType; // 2 full - 1 inc - 0 diff - BATU' inc 1 - diff 0
                    _logger.Verbose("30");


                    backupInfoList.Add(backupInfo);
                    _logger.Verbose("31");

                }
            }

                _logger.Verbose("32");
            return backupInfoList;
        }

        public BackupInfo GetBackupFile(BackupInfo backupInfo)
        {
            // seçilen dosyanın bilgilerini sağ tarafta kullanabilmek için
            _logger.Verbose("Get backup metodu çağırıldı. Parameter BackupInfo:{@BackupInfo}", backupInfo);
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
                    backupInfo.BackupTaskName = resultItem.TaskName;
                    backupInfo.Description = resultItem.TaskDescription;
                    backupInfo.BackupStorageInfo = backupInfo.BackupStorageInfo;
                    backupInfo.BackupStorageInfoId = backupInfo.BackupStorageInfoId;

                    backupInfo.Bootable = Convert.ToBoolean(resultItem.OSVolume);
                    backupInfo.VolumeSize = (long)resultItem.VolumeTotalSize;
                    backupInfo.StrVolumeSize = FormatBytes((long)resultItem.VolumeTotalSize);
                    backupInfo.UsedSize = (long)resultItem.VolumeUsedSize;
                    backupInfo.StrUsedSize = FormatBytes((long)resultItem.VolumeUsedSize);
                    backupInfo.FileSize = (long)resultItem.BytesNeedToCopy;
                    backupInfo.StrFileSize = FormatBytes((long)resultItem.BytesNeedToCopy);
                    backupInfo.FileName = resultItem.Fullpath.Split('\\').Last();
                    backupInfo.PCName = resultItem.ComputerName;
                    backupInfo.IpAddress = resultItem.IpAdress;
                    string createdDate = (resultItem.BackupDate.Day < 10) ? 0 + resultItem.BackupDate.Day.ToString() : resultItem.BackupDate.Day.ToString();
                    createdDate = createdDate + "." + ((resultItem.BackupDate.Month < 10) ? 0 + resultItem.BackupDate.Month.ToString() : resultItem.BackupDate.Month.ToString());
                    createdDate = createdDate + "." + resultItem.BackupDate.Year + " ";
                    createdDate = createdDate + ((resultItem.BackupDate.Hour < 10) ? 0 + resultItem.BackupDate.Hour.ToString() : resultItem.BackupDate.Hour.ToString());
                    createdDate = createdDate + ":" + ((resultItem.BackupDate.Minute < 10) ? 0 + resultItem.BackupDate.Minute.ToString() : resultItem.BackupDate.Minute.ToString());
                    createdDate = createdDate + ":" + resultItem.BackupDate.Second.ToString();
                    backupInfo.CreatedDate = createdDate;

                    if (resultItem.Version == -1)
                        backupInfo.Type = BackupTypes.Full;
                    else
                        backupInfo.Type = (BackupTypes)resultItem.BackupType; // 2 full - 1 inc - 0 diff - BATU' inc 1 - diff 0
                    return backupInfo;
                }
            }

            return null;
        }

        public bool RestoreBackupVolume(RestoreTask restoreTask)
        {
            _logger.Verbose("RestoreBackupVolume metodu çağırıldı");
            // hangi backup dosyası olduğu bulunup öyle verilmeli
            // rootDir = K:\O'yu K'ya Backup\Nar_BACKUP.nb
            string backupName = restoreTask.RootDir.Split('\\').Last();
            string newRootDir = restoreTask.RootDir.Substring(0, restoreTask.RootDir.Length - backupName.Length);

            var resultList = DiskTracker.CW_GetBackupsInDirectory(newRootDir);
            BackupMetadata backupMetadata = new BackupMetadata();

            foreach (var item in resultList)
            {
                if (item.Fullpath.Equals(restoreTask.RootDir))
                {
                    backupMetadata = item;
                    _logger.Verbose("|{@restoreTaskId}| restore taskı için restoreVolume gerçekleştirilecek.", restoreTask.Id);
                    return DiskTracker.CW_RestoreToVolume(restoreTask.TargetLetter[0], backupMetadata, true, newRootDir); //true gidecek
                }
            }

            _logger.Verbose("|{@restoreTaskId}| restore taskı için backupMetadata bulunamadı.", restoreTask.Id);
            return false;
            //return DiskTracker.CW_RestoreToVolume(restoreTask.TargetLetter[0], restoreTask.SourceLetter[0], restoreTask.BackupVersion, true, restoreTask.RootDir); //true gidecek
        }

        public bool RestoreBackupDisk(RestoreTask restoreTask)
        {
            // hangi backup dosyası olduğu bulunup öyle verilmeli
            // rootDir = K:\O'yu K'ya Backup\Nar_BACKUP.nb
            _logger.Verbose("RestoreBackupDisk metodu çağırıldı");
            string backupName = restoreTask.RootDir.Split('\\').Last();
            string newRootDir = restoreTask.RootDir.Substring(0, restoreTask.RootDir.Length - backupName.Length);

            var resultList = DiskTracker.CW_GetBackupsInDirectory(newRootDir);
            BackupMetadata backupMetadata = new BackupMetadata();

            foreach (var item in resultList)
            {
                if (item.Fullpath.Equals(restoreTask.RootDir))
                {
                    backupMetadata = item;
                    _logger.Verbose("|{@restoreTaskId}| restore taskı için restoreDisk gerçekleştirilecek.", restoreTask.Id);
                    return DiskTracker.CW_RestoreToFreshDisk(restoreTask.TargetLetter[0], backupMetadata, restoreTask.DiskId, newRootDir);
                }
            }

            _logger.Verbose("|{@restoreTaskId}| restore taskı için backupMetadata bulunamadı.", restoreTask.Id);
            return false;
            //return DiskTracker.CW_RestoreToFreshDisk(restoreTask.TargetLetter[0], restoreTask.SourceLetter[0], restoreTask.BackupVersion, restoreTask.DiskId, restoreTask.RootDir);
        }

        public bool CleanChain(char letter)
        {
            _logger.Verbose("CleanChain metodu çağırıldı");
            return _diskTracker.CW_RemoveFromTrack(letter);
        }

        public char AvailableVolumeLetter()
        {
            _logger.Verbose("AvailableVolumeLetter metodu çağırıldı");
            
            var returnValue = DiskTracker.CW_GetFirstAvailableVolumeLetter();
            _logger.Verbose("{return} GetFirstAvailableVolumeLetter()'den dönen harf.", returnValue);
            return returnValue;
        }

        public bool IsVolumeAvailable(char letter)
        {
            _logger.Verbose("IsVolumeAvailable metodu çağırıldı");
            return DiskTracker.CW_IsVolumeAvailable(letter);
        }

        public byte CreateIncDiffBackup(TaskInfo taskInfo) // 0 başarısız, 1 başarılı, 2 kullanıcı durdurdu
        {
            _logger.Verbose("CreateIncDiffBackup metodu çağırıldı");
            if (_statusInfoDal == null)
                throw new Exception("StatusInfoDal is null");
            var statusInfo = _statusInfoDal.Get(si => si.Id == taskInfo.StatusInfoId); //her task için uygulanmalı

            if (statusInfo == null)
                throw new Exception("statusInfo is null");

            if (_diskTracker == null)
                throw new Exception("_diskTracker is null");
            var manualResetEvent = new ManualResetEvent(true);
            _taskEventMap[taskInfo.Id] = manualResetEvent; 

             var timeElapsed = new Stopwatch();
            _timeElapsedMap[taskInfo.Id] = timeElapsed;

            timeElapsed.Start();

            _isStarted = true;
            _cancellationTokenSource[taskInfo.Id] = new CancellationTokenSource();

            var cancellationToken = _cancellationTokenSource[taskInfo.Id].Token;
            string letters = taskInfo.StrObje;

            int bufferSize = 64 * 1024 * 1024;
            byte[] buffer = new byte[bufferSize];
            StreamInfo str = new StreamInfo();
            long BytesReadSoFar = 0;
            int Read = 0;
            bool result = false;

            statusInfo.TaskName = taskInfo.Name;
            statusInfo.SourceObje = taskInfo.StrObje;

            // NAS için
            NetworkConnection nc = null;
            if (taskInfo.BackupStorageInfo.Type == BackupStorageType.NAS)
            {
                nc = new NetworkConnection(taskInfo.BackupStorageInfo.Path, taskInfo.BackupStorageInfo.Username, taskInfo.BackupStorageInfo.Password, taskInfo.BackupStorageInfo.Domain);
            }

            foreach (var letter in letters) // C D E F
            {
                if (_diskTracker.CW_SetupStream(letter, (int)taskInfo.BackupTaskInfo.Type, str)) // 0 diff, 1 inc, full (2) ucu gelmediğinden ayrılabilir veya aynı devam edebilir
                {
                    // yeterli alan kontrolü yap
                    if (!CheckDiskSize(taskInfo, statusInfo, timeElapsed, str, BytesReadSoFar))
                        return 3;

                    statusInfo.SourceObje = statusInfo.SourceObje + "-" + letter;
                    unsafe
                    {
                        fixed (byte* BAddr = &buffer[0])
                        {
                            FileStream file = File.Create(taskInfo.BackupStorageInfo.Path + str.FileName); //backupStorageInfo path alınıcak
                            while (true)
                            {
                                if (cancellationToken.IsCancellationRequested)
                                {
                                    //cleanup
                                    _diskTracker.CW_TerminateBackup(false, letter);
                                    _taskEventMap.Remove(taskInfo.Id);
                                    manualResetEvent.Dispose();
                                    _cancellationTokenSource[taskInfo.Id].Dispose();
                                    _cancellationTokenSource.Remove(taskInfo.Id);
                                    return 2;
                                }
                                manualResetEvent.WaitOne();

                                Read = _diskTracker.CW_ReadStream(BAddr, letter, bufferSize);
                                file.Write(buffer, 0, Read);
                                BytesReadSoFar += Read;

                                statusInfo.FileName = taskInfo.BackupStorageInfo.Path + str.MetadataFileName;

                                statusInfo.DataProcessed = BytesReadSoFar;
                                statusInfo.TotalDataProcessed = (long)str.CopySize;
                                statusInfo.AverageDataRate = ((statusInfo.TotalDataProcessed / 1024.0) / 1024.0) / (timeElapsed.ElapsedMilliseconds / 1000.0); // MB/s
                                statusInfo.InstantDataRate = ((BytesReadSoFar / 1024.0) / 1024.0) / (timeElapsed.ElapsedMilliseconds / 1000.0); // MB/s
                                statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
                                _statusInfoDal.Update(statusInfo);

                                if (Read != bufferSize)
                                    break;
                            }
                            result = (long)str.CopySize == BytesReadSoFar;
                            _diskTracker.CW_TerminateBackup(result, letter); //işlemi başarılı olup olmadığı cancel gelmeden
                            BytesReadSoFar = 0;

                            try
                            {
                                File.Copy(str.MetadataFileName, taskInfo.BackupStorageInfo.Path + str.MetadataFileName, false);                              
                            }
                            catch (IOException iox)
                            {
                                _logger.Error(iox, "{dizin} dizinine metadata dosyası kopyalama işlemi başarısız.", (taskInfo.BackupStorageInfo.Path + str.MetadataFileName));
                            }
                            try 
                            {
                                File.Delete(Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName);
                            }
                            catch (IOException ex)
                            {
                                _logger.Error(ex, "{dizin} dizinindeki metadata dosyasını silme işlemi başarısız.", (Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName));
                            }

                            file.Close();

                            if (Convert.ToBoolean(DiskTracker.CW_MetadataEditTaskandDescriptionField(taskInfo.BackupStorageInfo.Path + str.MetadataFileName, taskInfo.Name, taskInfo.Descripiton)))
                                _logger.Information("Metadata'ya görev bilgilerini yazma işlemi başarılı.");
                            else
                                _logger.Information("Metadata'ya görev bilgilerini yazma işlemi başarısız.");

                        }
                    }
                    statusInfo.SourceObje = statusInfo.SourceObje.Substring(0, statusInfo.SourceObje.Length - 2);
                }
                statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
                _statusInfoDal.Update(statusInfo);
            }

            // NAS için
            if (nc != null)
            {
                nc.Dispose();
            }

            manualResetEvent.Dispose();
            _taskEventMap.Remove(taskInfo.Id);

            _timeElapsedMap.Remove(taskInfo.Id);

            _cancellationTokenSource[taskInfo.Id].Dispose();
            _cancellationTokenSource.Remove(taskInfo.Id);
            if (result)
                return 1;
            return 0;
        }

        private bool CheckDiskSize(TaskInfo taskInfo, StatusInfo statusInfo, Stopwatch timeElapsed, StreamInfo str, long BytesReadSoFar)
        {
            List<DiskInformation> diskList = GetDiskList();
            foreach (var diskItem in diskList)
            {
                foreach (var volumeItem in diskItem.VolumeInfos)
                {
                    if (taskInfo.BackupStorageInfo.Type == BackupStorageType.Windows)
                    {
                        if (taskInfo.BackupStorageInfo.Path[0] == volumeItem.Letter)
                        {
                            if (volumeItem.FreeSize < ((long)str.CopySize + 2147483648))
                            {
                                _logger.Information("Volume Free Size: {FreeSize}, CopySize: {copySize}, ClusterSize: {cluster} ", volumeItem.FreeSize, str.CopySize, str.ClusterSize);

                                statusInfo.DataProcessed = BytesReadSoFar;
                                statusInfo.TotalDataProcessed = (long)str.CopySize;
                                statusInfo.AverageDataRate = 0; // MB/s
                                statusInfo.InstantDataRate = 0; // MB/s
                                statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
                                _statusInfoDal.Update(statusInfo);
                                return false;
                            }
                        }
                    }
                }
            }
            return true;
        }

        public bool CreateFullBackup(TaskInfo taskInfo) //bu method daha gelmedi 
        {
            _logger.Verbose("CreateFullBackup metodu çağırıldı");
            throw new NotImplementedException();
        }

        public void PauseTask(TaskInfo taskInfo)
        {
            _logger.Verbose("PauseTask metodu çağırıldı");

            var timeElapsed = _timeElapsedMap[taskInfo.Id];
            if (!_isStarted) throw new Exception("Backup is not started");
            timeElapsed.Stop();

            var statusInfo = _statusInfoDal.Get(si => si.Id == taskInfo.StatusInfoId);

            statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
            _statusInfoDal.Update(statusInfo);
            if (_taskEventMap.ContainsKey(taskInfo.Id))
            {
                _taskEventMap[taskInfo.Id].Reset();
            }
            taskInfo.Status = TaskStatusType.Paused;
            _taskInfoDal.Update(taskInfo);
        }

        public void CancelTask(TaskInfo taskInfo)
        {
            _logger.Verbose("CancelTask metodu çağırıldı");

            var timeElapsed = _timeElapsedMap[taskInfo.Id];
            _cancellationTokenSource[taskInfo.Id].Cancel();
            _isStarted = false;
            timeElapsed.Stop();

            var statusInfo = _statusInfoDal.Get(si => si.Id == taskInfo.StatusInfoId);

            statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
            _statusInfoDal.Update(statusInfo);

            timeElapsed.Reset();
            if (_taskEventMap.ContainsKey(taskInfo.Id))
            {
                _taskEventMap[taskInfo.Id].Set();
            }
            taskInfo.Status = TaskStatusType.Ready; 
            _taskInfoDal.Update(taskInfo);
        }

        public void ResumeTask(TaskInfo taskInfo)
        {
            _logger.Verbose("ResumeTask metodu çağırıldı");

            var timeElapsed = _timeElapsedMap[taskInfo.Id];
            if (!_isStarted) throw new Exception("Backup is not started");
            timeElapsed.Start();

            var statusInfo = _statusInfoDal.Get(si => si.Id == taskInfo.StatusInfoId);

            statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
            _statusInfoDal.Update(statusInfo);

            if (_taskEventMap.ContainsKey(taskInfo.Id))
            {
                _taskEventMap[taskInfo.Id].Set();
            }
            taskInfo.Status = TaskStatusType.Working;
            _taskInfoDal.Update(taskInfo);
        }

        public List<FilesInBackup> GetFileInfoList()
        {
            _logger.Verbose("GetFileInfoList metodu çağırıldı");

            var resultList = _cSNarFileExplorer.CW_GetFilesInCurrentDirectory();
            List<FilesInBackup> filesInBackupList = new List<FilesInBackup>();
            foreach (var item in resultList)
            {
                FilesInBackup filesInBackup = new FilesInBackup();
                filesInBackup.Id = (long)item.ID;
                filesInBackup.Name = item.Name;
                filesInBackup.Type = (FileType)Convert.ToInt16(item.IsDirectory); //Directory ise 1 
                filesInBackup.Size = (long)item.Size;
                filesInBackup.StrSize = FormatBytes((long)item.Size);

                filesInBackup.UpdatedDate = (item.LastModifiedTime.Day < 10) ? 0 + item.LastModifiedTime.Day.ToString() : item.LastModifiedTime.Day.ToString();
                filesInBackup.UpdatedDate = filesInBackup.UpdatedDate + "." +
                    ((item.LastModifiedTime.Month < 10) ? 0 + item.LastModifiedTime.Month.ToString() : item.LastModifiedTime.Month.ToString());
                filesInBackup.UpdatedDate = filesInBackup.UpdatedDate + "." + item.LastModifiedTime.Year + " ";
                filesInBackup.UpdatedDate = filesInBackup.UpdatedDate + ((item.LastModifiedTime.Hour < 10) ? 0 + item.LastModifiedTime.Hour.ToString() : item.LastModifiedTime.Hour.ToString());
                filesInBackup.UpdatedDate = filesInBackup.UpdatedDate + ":" +
                    ((item.LastModifiedTime.Minute < 10) ? 0 + item.LastModifiedTime.Minute.ToString() : item.LastModifiedTime.Minute.ToString());

                filesInBackupList.Add(filesInBackup);
            }
            return filesInBackupList;
        }

        public void FreeFileExplorer()
        {
            //ilk çağırdığımızda init ettiğimiz FileExplorer metodunu kapatmak/serbest bırakmak için
            _logger.Verbose("FreeFileExplorer metodu çağırıldı");
            _cSNarFileExplorer.CW_Free();
        }

        public bool GetSelectedFileInfo(FilesInBackup filesInBackup)
        {
            _logger.Verbose("GetSelectedFileInfo metodu çağırıldı");

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
            _logger.Verbose("PopDirectory metodu çağırıldı");
            _cSNarFileExplorer.CW_PopDirectory();
        }

        public string GetCurrentDirectory()
        {
            _logger.Verbose("GetCurrentDirectory metodu çağırıldı");
            return _cSNarFileExplorer.CW_GetCurrentDirectoryString();
        }

        public List<ActivityDownLog> GetDownLogList() //bu method daha gelmedi
        {
            _logger.Verbose("GetDownLogList metodu çağırıldı");

            List<ActivityDownLog> logList = new List<ActivityDownLog>();
            foreach (var item in DiskTracker.CW_GetLogs())
            {
                string logDate = (item.Time.Day < 10) ? 0 + item.Time.Day.ToString() : item.Time.Day.ToString();
                logDate += "." + ((item.Time.Month < 10) ? 0 + item.Time.Month.ToString() : item.Time.Month.ToString());
                logDate += "." + item.Time.Year + " ";
                logDate += ((item.Time.Hour < 10) ? 0 + item.Time.Hour.ToString() : item.Time.Hour.ToString());
                logDate += ":" + ((item.Time.Minute < 10) ? 0 + item.Time.Minute.ToString() : item.Time.Minute.ToString());
                logDate += ":" + ((item.Time.Second < 10) ? 0 + item.Time.Second.ToString() : item.Time.Second.ToString());

                logList.Add(new ActivityDownLog
                {
                    Detail = item.LogStr.Trim(),
                    Time = logDate
                });
            }
            return logList;
        }

        public void RestoreFilesInBackup(int fileId, string backupDirectory, string targetDirectory) // batuhan hangi backup olduğunu nasıl anlayacak? backup directoryde backup ismi almıyor
        {
            _logger.Verbose("RestoreFilesInBackup metodu çağırıldı");

            //_cSNarFileExplorer.CW_RestoreFile(ID, seçilen backup'ın directorysi dosya ismi olmadan, yüklenecek yer)
            //void CW_RestoreFile(INT64 ID);
            //_cSNarFileExplorer.CW_Free FileExplorer kapatıldığında Free çağırmakta fayda var bellek yönetimi için tutulan alanları geri veriyor sisteme
            /*Geri yükle fileExplorerdan istenen dosyayı geri yüklemek için
            cSNarFileExplorer.CW_RestoreFile(dosyaid, Backupdirectory (ilgil backup hariç), kaydedilecekyol)*/
            _cSNarFileExplorer.CW_RestoreFile(fileId, backupDirectory, targetDirectory);
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
