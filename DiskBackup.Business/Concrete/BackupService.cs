using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.DataAccess.Core;
using DiskBackup.Entities.Concrete;
using NarDIWrapper;
using Serilog;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
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
        private CSNarFileExplorer _cSNarFileExplorer;

        private Dictionary<int, ManualResetEvent> _taskEventMap = new Dictionary<int, ManualResetEvent>(); // aynı işlem Stopwatch ve cancellationtoken source için de yapılacak ancak Quartz'ın pause, resume ve cancel işlemleri düzgün çalışıyorsa kullanılmayacak
        private Dictionary<int, CancellationTokenSource> _cancellationTokenSource = new Dictionary<int, CancellationTokenSource>();
        private Dictionary<int, Stopwatch> _timeElapsedMap = new Dictionary<int, Stopwatch>();

        private bool _initTrackerResult = false;
        private bool _refreshIncDiffTaskFlag = false;
        private bool _refreshIncDiffLogFlag = false; // Refreshde yenilememe durumu olması durumunda her task için ayrı flagler oluşturulacak

        private readonly IStatusInfoDal _statusInfoDal; // status bilgilerini veritabanına yazabilmek için gerekli
        private readonly ITaskInfoDal _taskInfoDal; // status bilgilerini veritabanına yazabilmek için gerekli
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IBackupTaskDal _backupTaskDal;
        private readonly IConfigurationDataDal _configurationDataDal;
        private readonly ILogger _logger;

        private NetworkConnection _nc;

        public BackupService(IStatusInfoDal statusInfoRepository, ITaskInfoDal taskInfoDal, ILogger logger, IBackupStorageDal backupStorageDal, IBackupTaskDal backupTaskDal, IConfigurationDataDal configurationDataDal)
        {
            _statusInfoDal = statusInfoRepository;
            _taskInfoDal = taskInfoDal;
            _logger = logger.ForContext<BackupService>();
            _backupStorageDal = backupStorageDal;
            _backupTaskDal = backupTaskDal;
            _configurationDataDal = configurationDataDal;
        }

        public bool InitTracker()
        {
            // programla başlat 1 kere çalışması yeter
            // false değeri dönüyor ise eğer backup işlemlerini disable et
            _logger.Verbose("InitTracker metodu çağırıldı");

            try
            {
                _initTrackerResult = _diskTracker.CW_InitTracker();
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "_diskTracker.CW_InitTracker() gerçekleştirilemedi.");
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

        public void InitFileExplorer(BackupInfo backupInfo)
        {
            _logger.Verbose("InitFileExplorer metodu çağırıldı");
            _logger.Verbose("İnitFileExplorer: {path}, {name}", backupInfo.BackupStorageInfo.Path, backupInfo.MetadataFileName);
            var backupStorageInfo = _backupStorageDal.Get(x => x.Id == backupInfo.BackupStorageInfoId);
            if (backupStorageInfo.Type == BackupStorageType.NAS)
            {
                try
                {
                    if (backupStorageInfo.Type == BackupStorageType.NAS)
                    {
                        _nc = new NetworkConnection(backupStorageInfo.Path.Substring(0, backupStorageInfo.Path.Length - 1), backupStorageInfo.Username, backupStorageInfo.Password, backupStorageInfo.Domain);
                    }
                }
                catch (Exception ex)
                {
                    _logger.Error(ex, "Uzak paylaşıma bağlanılamadığı için file explorer açılamıyor.");
                    return;
                }
            }

            try
            {
                _cSNarFileExplorer = new CSNarFileExplorer(backupInfo.BackupStorageInfo.Path + backupInfo.MetadataFileName);
            }
            catch (Exception ex)
            {
                _logger.Error(ex, $"Yeni CSNarFileExplorer oluşturulamadı. Path: {backupInfo.BackupStorageInfo.Path + backupInfo.MetadataFileName}");
            }
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
                            volumeInfo.HealthStatu = HealthSituation.Unhealthy;
                        }
                        else if (volumeItem.DiskType == 'M')
                        {
                            volumeInfo.DiskType = "MBR";
                            volumeInfo.HealthStatu = HealthSituation.Healthy;
                        }
                        else if (volumeItem.DiskType == 'G')
                        {
                            volumeInfo.DiskType = "GPT";
                            volumeInfo.HealthStatu = HealthSituation.Healthy;
                        }
                        diskList[index].VolumeInfos.Add(volumeInfo);
                    }
                }

                // raw, boş disk vs kontrolü
                if (diskList[index].VolumeInfos.Count < 1)
                {
                    diskList.Remove(temp);
                    index--;
                }

                index++;
            }
            return diskList;
        }

        public List<BackupInfo> GetBackupFileList(List<BackupStorageInfo> backupStorageList)
        {
            _logger.Verbose("GetBackupFileList metodu çağırıldı");
            List<BackupInfo> backupInfoList = new List<BackupInfo>();
            //bootable = osVolume (true)
            int index = 0;
            foreach (BackupStorageInfo backupStorageItem in backupStorageList)
            {
                //Aynı path mi kontrolü
                bool controlFlag = true;
                for (int i = index - 1; i >= 0; i--)
                {
                    if (backupStorageItem.Path == backupStorageList[i].Path)
                    {
                        controlFlag = false;
                        break;
                    }
                }
                index++;

                if (controlFlag)
                {
                    NetworkConnection nc = null;
                    if (backupStorageItem.Type == BackupStorageType.NAS)
                    {
                        try
                        {
                            nc = new NetworkConnection(backupStorageItem.Path.Substring(0, backupStorageItem.Path.Length - 1), backupStorageItem.Username, backupStorageItem.Password, backupStorageItem.Domain);
                        }
                        catch (Exception ex)
                        {
                            _logger.Error(ex, "Uzak paylaşıma bağlanılamadığı için backup dosyaları gösterilemiyor. {path}", backupStorageItem.Path);
                            continue;
                        }
                    }

                    var returnList = DiskTracker.CW_GetBackupsInDirectory(backupStorageItem.Path);
                    foreach (var returnItem in returnList)
                    {
                        BackupInfo backupInfo = ConvertToBackupInfo(backupStorageItem, returnItem);
                        backupInfoList.Add(backupInfo);
                    }

                    if (nc != null)
                        nc.Dispose();
                }
            }

            return backupInfoList;
        }

        public BackupInfo GetBackupFile(BackupInfo backupInfo)
        {
            // Bu fonksiyonu kullanır ve hata alınır ise ilk bakılması gereken yer BackupStorageIdleri
            _logger.Verbose("Get backup metodu çağırıldı. Parameter BackupInfo:{@BackupInfo}", backupInfo);
            var result = DiskTracker.CW_GetBackupsInDirectory(backupInfo.BackupStorageInfo.Path);

            foreach (var resultItem in result)
            {
                if (resultItem.Version == backupInfo.Version && resultItem.BackupType.Equals(backupInfo.Type) && resultItem.DiskType.Equals(backupInfo.DiskType))
                {
                    _logger.Verbose("-------------" + backupInfo.BackupStorageInfo.Id + " " + backupInfo.BackupStorageInfo.StorageName);
                    return ConvertToBackupInfo(backupInfo.BackupStorageInfo, resultItem);
                }
            }

            return null;
        }

        public byte RestoreBackupVolume(TaskInfo taskInfo)
        {
            _logger.Verbose("RestoreBackupVolume metodu çağırıldı");

            if (!GetInitTracker())
                return 4;

            // rootDir = K:\O'yu K'ya Backup\Nar_BACKUP.nb -- hangi backup dosyası olduğu bulunup öyle verilmeli
            NetworkConnection nc = null;
            if (taskInfo.BackupStorageInfo.Type == BackupStorageType.NAS)
            {
                try
                {
                    nc = new NetworkConnection(taskInfo.BackupStorageInfo.Path.Substring(0, taskInfo.BackupStorageInfo.Path.Length - 1), taskInfo.BackupStorageInfo.Username, taskInfo.BackupStorageInfo.Password, taskInfo.BackupStorageInfo.Domain);
                }
                catch (Exception ex)
                {
                    _logger.Error(ex, "Uzak paylaşıma bağlanılamadığı için restore gerçekleştirilemiyor. {path}", taskInfo.BackupStorageInfo.Path);
                    return 2;
                }
            }

            var timeElapsed = new Stopwatch();
            _timeElapsedMap[taskInfo.Id] = timeElapsed;
            timeElapsed.Start();

            string backupName = taskInfo.RestoreTaskInfo.RootDir.Split('\\').Last();
            string newRootDir = taskInfo.RestoreTaskInfo.RootDir.Substring(0, taskInfo.RestoreTaskInfo.RootDir.Length - backupName.Length);
            var resultList = DiskTracker.CW_GetBackupsInDirectory(newRootDir);
            BackupMetadata backupMetadata = new BackupMetadata();
            var statusInfo = _statusInfoDal.Get(x => x.Id == taskInfo.StatusInfoId);

            foreach (var item in resultList)
            {
                if (item.Fullpath.Equals(taskInfo.RestoreTaskInfo.RootDir))
                {
                    backupMetadata = item;
                    _logger.Verbose("|{@restoreTaskId}| restore taskı için restoreVolume gerçekleştirilecek.", taskInfo.RestoreTaskInfo.Id);

                    if (ChainInTheSameDirectory(resultList, backupMetadata))
                    {
                        //return Convert.ToByte(DiskTracker.CW_RestoreToVolume(taskInfo.RestoreTaskInfo.TargetLetter[0], backupMetadata, true, newRootDir)); //true gidecek
                        RestoreStream restoreStream = new RestoreStream(backupMetadata, newRootDir);
                        restoreStream.SetupStream(taskInfo.RestoreTaskInfo.TargetLetter[0]);
                        ulong bytesReadSoFar = 0;
                        statusInfo.TotalDataProcessed = (long)restoreStream.BytesNeedToCopy;

                        bool result = false;
                        _logger.Information($"bytesReadSoFar: {bytesReadSoFar} - bytesToBeCopied: {restoreStream.BytesNeedToCopy} - result: {result} - result.toByte: {Convert.ToByte(result)}");
                        // anlık veri için
                        Stopwatch passingTime = new Stopwatch();
                        long instantProcessData = 0;
                        passingTime.Start();

                        var manualResetEvent = new ManualResetEvent(true);
                        _taskEventMap[taskInfo.Id] = manualResetEvent;
                        _cancellationTokenSource[taskInfo.Id] = new CancellationTokenSource();
                        var cancellationToken = _cancellationTokenSource[taskInfo.Id].Token;

                        while (true)
                        {
                            if (cancellationToken.IsCancellationRequested)
                            {
                                restoreStream.TerminateRestore();
                                _taskEventMap.Remove(taskInfo.Id);
                                manualResetEvent.Dispose();
                                _cancellationTokenSource[taskInfo.Id].Dispose();
                                _cancellationTokenSource.Remove(taskInfo.Id);
                                return 5;
                            }
                            manualResetEvent.WaitOne();

                            var read = restoreStream.CW_AdvanceStream();
                            bytesReadSoFar += read;

                            instantProcessData += (long)read; // anlık veri için
                            if (passingTime.ElapsedMilliseconds > 500)
                            {
                                if (statusInfo.TotalDataProcessed >= (long)bytesReadSoFar)
                                    statusInfo.DataProcessed = (long)bytesReadSoFar;
                                statusInfo.AverageDataRate = ((statusInfo.TotalDataProcessed / 1024.0) / 1024.0) / (timeElapsed.ElapsedMilliseconds / 1000.0); // MB/s
                                if (instantProcessData != 0) // anlık veri için 0 gelmesin diye
                                    statusInfo.InstantDataRate = ((instantProcessData / 1024.0) / 1024.0) / (passingTime.ElapsedMilliseconds / 1000.0); // MB/s
                                statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
                                _statusInfoDal.Update(statusInfo);
                                if (passingTime.ElapsedMilliseconds > 1000) // anlık veri için her saniye güncellensin diye
                                {
                                    instantProcessData = 0;
                                    passingTime.Restart();
                                }
                            }

                            if (read == 0) // 0 dönerse restore bitti demektir
                            {
                                result = restoreStream.CheckStreamStatus(); // başarılı ise true, değilse false dönecek
                                _logger.Information($"read = 0 geldi. result: {result}");
                                break;
                            }
                            if (!restoreStream.CheckStreamStatus())
                            {
                                result = false;
                                _logger.Information($"check stream false geldi. result: {false}");
                                break;
                            }
                        }
                        restoreStream.TerminateRestore();
                        timeElapsed.Stop();
                        _timeElapsedMap.Remove(taskInfo.Id);

                        manualResetEvent.Dispose();
                        _taskEventMap.Remove(taskInfo.Id);
                        _cancellationTokenSource[taskInfo.Id].Dispose();
                        _cancellationTokenSource.Remove(taskInfo.Id);
                        if (result)
                        {
                            statusInfo.TotalDataProcessed = statusInfo.DataProcessed;
                            _statusInfoDal.Update(statusInfo);
                        }
                        return Convert.ToByte(result);
                    }
                }
            }

            if (nc != null)
                nc.Dispose();

            _logger.Verbose("|{@restoreTaskId}| restore taskı için backupMetadata bulunamadı.", taskInfo.RestoreTaskInfo.Id);
            timeElapsed.Stop();
            _timeElapsedMap.Remove(taskInfo.Id);
            return 3;
        }

        // Tüm dosyalar aynı dizinde mi kontrolü yap
        private bool ChainInTheSameDirectory(List<BackupMetadata> backupMetadataList, BackupMetadata backupMetadata)
        {
            _logger.Verbose("ChainInTheSameDirectory metodu çağırıldı");

            if (backupMetadata.Version != -1)
            {
                List<BackupMetadata> chainList = GetChainList(backupMetadata, backupMetadataList);

                if (backupMetadata.BackupType == 0) // 0 diff - diff restore
                {
                    // Full ve restore edilecek backup aynı dizinde olması gerekiyor.
                    if (chainList[0].Version == -1)
                        return true;
                    else
                        return false;
                }
                else if (backupMetadata.BackupType == 1) // 1 inc - inc restore
                {
                    // ilgili backup versiyonuna kadar tüm backuplar aynı dizinde olmalı. Sayıyı kontrol et, versiyonları kontrol et
                    if (chainList.Count >= backupMetadata.Version + 2)
                    {
                        for (int i = 0; i <= backupMetadata.Version + 1; i++)
                        {
                            if (!(chainList[i].Version == i - 1))
                                return false;
                        }
                        return true;
                    }
                    else
                        return false;
                }
            }
            return true;
        }

        public byte RestoreBackupDisk(TaskInfo taskInfo)
        {
            _logger.Verbose("RestoreBackupDisk metodu çağırıldı");

            if (!GetInitTracker())
                return 4;

            // rootDir = K:\O'yu K'ya Backup\Nar_BACKUP.nb -- hangi backup dosyası olduğu bulunup öyle verilmeli
            NetworkConnection nc = null;
            if (taskInfo.BackupStorageInfo.Type == BackupStorageType.NAS)
            {
                try
                {
                    nc = new NetworkConnection(taskInfo.BackupStorageInfo.Path.Substring(0, taskInfo.BackupStorageInfo.Path.Length - 1), taskInfo.BackupStorageInfo.Username, taskInfo.BackupStorageInfo.Password, taskInfo.BackupStorageInfo.Domain);
                }
                catch (Exception ex)
                {
                    _logger.Error(ex, "Uzak paylaşıma bağlanılamadığı için restore gerçekleştirilemiyor. {path}", taskInfo.BackupStorageInfo.Path);
                    return 2;
                }
            }

            var timeElapsed = new Stopwatch();
            _timeElapsedMap[taskInfo.Id] = timeElapsed;
            timeElapsed.Start();

            string backupName = taskInfo.RestoreTaskInfo.RootDir.Split('\\').Last();
            string newRootDir = taskInfo.RestoreTaskInfo.RootDir.Substring(0, taskInfo.RestoreTaskInfo.RootDir.Length - backupName.Length);
            var resultList = DiskTracker.CW_GetBackupsInDirectory(newRootDir);
            BackupMetadata backupMetadata = new BackupMetadata();
            var statusInfo = _statusInfoDal.Get(x => x.Id == taskInfo.StatusInfoId);

            foreach (var item in resultList)
            {
                if (item.Fullpath.Equals(taskInfo.RestoreTaskInfo.RootDir))
                {
                    backupMetadata = item;
                    _logger.Verbose("|{@restoreTaskId}| restore taskı için restoreDisk gerçekleştirilecek.", taskInfo.RestoreTaskInfo.Id);

                    if (ChainInTheSameDirectory(resultList, backupMetadata))
                    {
                        RestoreStream restoreStream = new RestoreStream(backupMetadata, newRootDir);
                        SettingBootable(taskInfo, backupMetadata, restoreStream);
                        restoreStream.SetupStream(taskInfo.RestoreTaskInfo.TargetLetter[0]);
                        ulong bytesReadSoFar = 0;
                        statusInfo.TotalDataProcessed = (long)restoreStream.BytesNeedToCopy;

                        bool result = false;
                        _logger.Information($"bytesReadSoFar: {bytesReadSoFar} - bytesToBeCopied: {restoreStream.BytesNeedToCopy} - result: {result} - result.toByte: {Convert.ToByte(result)}");
                        // anlık veri için
                        Stopwatch passingTime = new Stopwatch();
                        long instantProcessData = 0;
                        passingTime.Start();

                        var manualResetEvent = new ManualResetEvent(true);
                        _taskEventMap[taskInfo.Id] = manualResetEvent;
                        _cancellationTokenSource[taskInfo.Id] = new CancellationTokenSource();
                        var cancellationToken = _cancellationTokenSource[taskInfo.Id].Token;

                        while (true)
                        {
                            if (cancellationToken.IsCancellationRequested)
                            {
                                restoreStream.TerminateRestore();
                                _taskEventMap.Remove(taskInfo.Id);
                                manualResetEvent.Dispose();
                                _cancellationTokenSource[taskInfo.Id].Dispose();
                                _cancellationTokenSource.Remove(taskInfo.Id);
                                return 5;
                            }
                            manualResetEvent.WaitOne();

                            var read = restoreStream.CW_AdvanceStream();
                            bytesReadSoFar += read;

                            instantProcessData += (long)read; // anlık veri için
                            if (passingTime.ElapsedMilliseconds > 500)
                            {
                                if (statusInfo.TotalDataProcessed >= (long)bytesReadSoFar)
                                    statusInfo.DataProcessed = (long)bytesReadSoFar;
                                statusInfo.AverageDataRate = ((statusInfo.TotalDataProcessed / 1024.0) / 1024.0) / (timeElapsed.ElapsedMilliseconds / 1000.0); // MB/s
                                if (instantProcessData != 0) // anlık veri için 0 gelmesin diye
                                    statusInfo.InstantDataRate = ((instantProcessData / 1024.0) / 1024.0) / (passingTime.ElapsedMilliseconds / 1000.0); // MB/s
                                statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
                                _statusInfoDal.Update(statusInfo);
                                if (passingTime.ElapsedMilliseconds > 1000) // anlık veri için her saniye güncellensin diye
                                {
                                    instantProcessData = 0;
                                    passingTime.Restart();
                                }
                            }

                            if (read == 0) // 0 dönerse restore bitti demektir
                            {
                                result = restoreStream.CheckStreamStatus(); // başarılı ise true, değilse false dönecek
                                _logger.Information($"read = 0 geldi. result: {result}");
                                break;
                            }
                            if (!restoreStream.CheckStreamStatus())
                            {
                                result = false;
                                _logger.Information($"check stream false geldi. result: {false}");
                                break;
                            }
                        }
                        restoreStream.TerminateRestore();
                        timeElapsed.Stop();
                        _timeElapsedMap.Remove(taskInfo.Id);

                        manualResetEvent.Dispose();
                        _taskEventMap.Remove(taskInfo.Id);
                        _cancellationTokenSource[taskInfo.Id].Dispose();
                        _cancellationTokenSource.Remove(taskInfo.Id);
                        if (result)
                        {
                            statusInfo.TotalDataProcessed = statusInfo.DataProcessed;
                            _statusInfoDal.Update(statusInfo);
                        }
                        return Convert.ToByte(result);
                    }
                }
            }

            if (nc != null)
                nc.Dispose();

            _logger.Verbose("|{@restoreTaskId}| restore taskı için backupMetadata bulunamadı.", taskInfo.RestoreTaskInfo.Id);
            timeElapsed.Stop();
            _timeElapsedMap.Remove(taskInfo.Id);
            return 3;
        }

        private void SettingBootable(TaskInfo taskInfo, BackupMetadata backupMetadata, RestoreStream restoreStream)
        {
            if (taskInfo.RestoreTaskInfo.Bootable == RestoreBootable.NotBootable)
            {
                _logger.Information("not bootable");
                restoreStream.SetDiskRestore(taskInfo.RestoreTaskInfo.DiskId, taskInfo.RestoreTaskInfo.TargetLetter[0], false, false, backupMetadata.DiskType);
                //return Convert.ToByte(DiskTracker.CW_RestoreToFreshDisk(taskInfo.RestoreTaskInfo.TargetLetter[0], backupMetadata, taskInfo.RestoreTaskInfo.DiskId, newRootDir, false, false, backupMetadata.DiskType));
            }
            else if (taskInfo.RestoreTaskInfo.Bootable == RestoreBootable.MBR)
            {
                if (backupMetadata.DiskType.Equals('M'))
                {
                    _logger.Information("MBR - M");
                    restoreStream.SetDiskRestore(taskInfo.RestoreTaskInfo.DiskId, taskInfo.RestoreTaskInfo.TargetLetter[0], true, false, backupMetadata.DiskType);
                    //return Convert.ToByte(DiskTracker.CW_RestoreToFreshDisk(taskInfo.RestoreTaskInfo.TargetLetter[0], backupMetadata, taskInfo.RestoreTaskInfo.DiskId, newRootDir, true, false, backupMetadata.DiskType));
                }
                else
                {
                    _logger.Information("MBR - G");
                    restoreStream.SetDiskRestore(taskInfo.RestoreTaskInfo.DiskId, taskInfo.RestoreTaskInfo.TargetLetter[0], true, true, 'M');
                    //return Convert.ToByte(DiskTracker.CW_RestoreToFreshDisk(taskInfo.RestoreTaskInfo.TargetLetter[0], backupMetadata, taskInfo.RestoreTaskInfo.DiskId, newRootDir, true, true, 'M'));
                }
            }
            else if (taskInfo.RestoreTaskInfo.Bootable == RestoreBootable.GPT)
            {
                if (backupMetadata.DiskType.Equals('G'))
                {
                    _logger.Information("GPT - G");
                    restoreStream.SetDiskRestore(taskInfo.RestoreTaskInfo.DiskId, taskInfo.RestoreTaskInfo.TargetLetter[0], true, false, backupMetadata.DiskType);
                    //return Convert.ToByte(DiskTracker.CW_RestoreToFreshDisk(taskInfo.RestoreTaskInfo.TargetLetter[0], backupMetadata, taskInfo.RestoreTaskInfo.DiskId, newRootDir, true, false, backupMetadata.DiskType));
                }
                else
                {
                    _logger.Information("GPT - M");
                    restoreStream.SetDiskRestore(taskInfo.RestoreTaskInfo.DiskId, taskInfo.RestoreTaskInfo.TargetLetter[0], true, true, 'G');
                    //return Convert.ToByte(DiskTracker.CW_RestoreToFreshDisk(taskInfo.RestoreTaskInfo.TargetLetter[0], backupMetadata, taskInfo.RestoreTaskInfo.DiskId, newRootDir, true, true, 'G'));
                }
            }
            else
            {
                _logger.Information("SON ELSE");
            }
        }

        public bool CleanChain(char letter)
        {
            _logger.Verbose("CleanChain metodu çağırıldı");
            _logger.Information("{letter} zinciri temizleniyor.", letter);
            try
            {
                return _diskTracker.CW_RemoveFromTrack(letter);
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "_diskTracker.CW_RemoveFromTrack() gerçekleştirilemedi.");
            }
            return false;
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

        public int CreateIncDiffBackup(TaskInfo taskInfo) // 0 başarısız, 1 başarılı, 2 kullanıcı durdurdu
        {
            _logger.Verbose("CreateIncDiffBackup metodu çağırıldı");

            if (!GetInitTracker())
                return 5;

            // yeni zincirin başlayıp başlamayacağına dair kontroller gerçekleştirilip duruma göre CleanChain çağırılıyor
            int fullCount = 0;
            int days = taskInfo.BackupTaskInfo.FullBackupTime;
            if (taskInfo.BackupTaskInfo.FullBackupTimeType == FullBackupTimeTyp.Week)
            {
                days = taskInfo.BackupTaskInfo.FullBackupTime * 7;
            }
            if ((DateTime.Now - taskInfo.BackupTaskInfo.LastFullBackupDate).Days >= days)
            {
                foreach (var letter in taskInfo.StrObje)
                {
                    try
                    {
                        CleanChain(letter);
                    }
                    catch (Exception ex)
                    {
                        _logger.Error(ex, "Beklenmedik hatadan dolayı {harf} zincir temizleme işlemi gerçekleştirilemedi.", letter);
                        return 8;
                    }
                }
            }

            // NAS için
            NetworkConnection nc = null;
            try
            {
                if (taskInfo.BackupStorageInfo.Type == BackupStorageType.NAS)
                {
                    nc = new NetworkConnection(taskInfo.BackupStorageInfo.Path.Substring(0, taskInfo.BackupStorageInfo.Path.Length - 1), taskInfo.BackupStorageInfo.Username, taskInfo.BackupStorageInfo.Password, taskInfo.BackupStorageInfo.Domain);
                }
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Uzak paylaşıma bağlanılamadığı için backup işlemine devam edilemiyor. {path}", taskInfo.BackupStorageInfo.Path);
                return 4;
            }

            if (!Directory.Exists(taskInfo.BackupStorageInfo.Path))
                return 6;

            var statusInfo = _statusInfoDal.Get(si => si.Id == taskInfo.StatusInfoId); //her task için uygulanmalı

            var manualResetEvent = new ManualResetEvent(true);
            _taskEventMap[taskInfo.Id] = manualResetEvent;

            var timeElapsed = new Stopwatch();
            _timeElapsedMap[taskInfo.Id] = timeElapsed;
            timeElapsed.Start();

            _cancellationTokenSource[taskInfo.Id] = new CancellationTokenSource();
            var cancellationToken = _cancellationTokenSource[taskInfo.Id].Token;
            string letters = taskInfo.StrObje;

            int bufferSize = DiskTracker.CW_HintBufferSize();
            byte[] buffer = new byte[bufferSize];
            StreamInfo str = new StreamInfo();
            long bytesReadSoFar = 0;
            bool result = false;

            statusInfo.TaskName = taskInfo.Name;
            statusInfo.SourceObje = taskInfo.StrObje;

            foreach (var letter in letters) // C D E F
            {
                // anlık veri için
                Stopwatch passingTime = new Stopwatch();
                long instantProcessData = 0;
                passingTime.Start();

                _logger.Information($"{letter} backup işlemi başlatılıyor...");
                if (_diskTracker.CW_SetupStream(letter, (int)taskInfo.BackupTaskInfo.Type, str, true)) // 0 diff, 1 inc, full (2) ucu gelmediğinden ayrılabilir veya aynı devam edebilir
                {
                    // yeterli alan kontrolü yap
                    if (!CheckDiskSize(taskInfo, statusInfo, timeElapsed, str, bytesReadSoFar))
                        return 3;

                    statusInfo.SourceObje = statusInfo.SourceObje + "-" + letter;
                    unsafe
                    {
                        fixed (byte* BAddr = &buffer[0])
                        {
                            FileStream file = File.Create(taskInfo.BackupStorageInfo.Path + str.FileName); //backupStorageInfo path alınıcak
                            statusInfo.TotalDataProcessed = (long)str.CopySize;

                            if (str.FileName.Contains("FULL")) // Süreli backup zinciri için ne zaman en son full backup'ın alındığı bu count ile belirleniyor return 1'den önce tarih güncelleniyor
                            {
                                fullCount++;
                            }

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
                                    file.Close();
                                    // oluşturulan dosyayı sil
                                    _logger.Information("Görev iptal edildiği için {dizin} ve {metadataDizin} dizinleri siliniyor.", taskInfo.BackupStorageInfo.Path + str.FileName, (Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName));
                                    DeleteBrokenBackupFiles(taskInfo, str);
                                    return 2;
                                }
                                manualResetEvent.WaitOne();

                                var readStream = _diskTracker.CW_ReadStream(BAddr, letter, bufferSize);
                                file.Write(buffer, 0, (int)readStream.WriteSize);
                                bytesReadSoFar += readStream.DecompressedSize;

                                instantProcessData += readStream.DecompressedSize; // anlık veri için              
                                if (passingTime.ElapsedMilliseconds > 500)
                                {
                                    statusInfo.FileName = taskInfo.BackupStorageInfo.Path + str.FileName;
                                    if (statusInfo.TotalDataProcessed >= (long)bytesReadSoFar)
                                        statusInfo.DataProcessed = bytesReadSoFar;
                                    statusInfo.AverageDataRate = ((statusInfo.TotalDataProcessed / 1024.0) / 1024.0) / (timeElapsed.ElapsedMilliseconds / 1000.0); // MB/s
                                    if (instantProcessData != 0) // anlık veri için 0 gelmesin diye
                                        statusInfo.InstantDataRate = ((instantProcessData / 1024.0) / 1024.0) / (passingTime.ElapsedMilliseconds / 1000.0); // MB/s
                                    statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
                                    _statusInfoDal.Update(statusInfo);

                                    if (passingTime.ElapsedMilliseconds > 1000) // anlık veri için her saniye güncellensin diye
                                    {
                                        instantProcessData = 0;
                                        passingTime.Restart();
                                    }
                                }

                                if (readStream.WriteSize == 0 || !_diskTracker.CW_CheckStreamStatus(letter))
                                {
                                    _logger.Information($"readStream.WriteSize: {readStream.WriteSize} - readStream.Error: {readStream.Error} - _diskTracker.CW_CheckStreamStatus({letter}): {_diskTracker.CW_CheckStreamStatus(letter)}");
                                    break;
                                }
                            }
                            result = _diskTracker.CW_CheckStreamStatus(letter);
                            _logger.Information($"_diskTracker.CW_CheckStreamStatus({letter}): {result}");
                            _diskTracker.CW_TerminateBackup(result, letter); //işlemi başarılı olup olmadığı cancel gelmeden
                            bytesReadSoFar = 0;

                            CopyAndDeleteMetadataFile(taskInfo, str); //çalışılan dizine çıkartılan narmd dosyası kopyalanıp ilgili dizine silme işlemi yapılıyor

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

            timeElapsed.Reset();
            _timeElapsedMap.Remove(taskInfo.Id);

            _cancellationTokenSource[taskInfo.Id].Dispose();
            _cancellationTokenSource.Remove(taskInfo.Id);
            if (result)
            {
                statusInfo.TotalDataProcessed = statusInfo.DataProcessed;
                _statusInfoDal.Update(statusInfo);
                if (fullCount == taskInfo.Obje)
                {
                    taskInfo.BackupTaskInfo.LastFullBackupDate = (taskInfo.LastWorkingDate - TimeSpan.FromSeconds(taskInfo.LastWorkingDate.Second)) - TimeSpan.FromMilliseconds(taskInfo.LastWorkingDate.Millisecond); // ms ve sn sıfırlamak için
                    _backupTaskDal.Update(taskInfo.BackupTaskInfo);
                }
                return 1;
            }
            _logger.Information("Görev başarısız olduğu için {dizin} ve {metadataDizin} dizinleri siliniyor.", taskInfo.BackupStorageInfo.Path + str.FileName, (Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName));
            DeleteBrokenBackupFiles(taskInfo, str);
            return 0;
        }

        private unsafe void CopyAndDeleteMetadataFile(TaskInfo taskInfo, StreamInfo str)
        {
            try
            {
                if (File.Exists(Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName))
                {
                    File.Copy(str.MetadataFileName, taskInfo.BackupStorageInfo.Path + str.MetadataFileName, false);
                    File.Delete(Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName);
                }
                else
                    _logger.Information($"{Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName} ilgili .narmd uzantılı dosya bulunamadığı için kopyalama işlemi yapılamıyor ve silinemiyor.");
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "{dizin} dizinine metadata dosyası kopyalayıp silme işlemi başarısız.", (taskInfo.BackupStorageInfo.Path + str.MetadataFileName));
            }
        }

        private void DeleteBrokenBackupFiles(TaskInfo taskInfo, StreamInfo str)
        {
            try
            {
                if (File.Exists(taskInfo.BackupStorageInfo.Path + str.FileName))
                    File.Delete(taskInfo.BackupStorageInfo.Path + str.FileName);
                else
                    _logger.Information($"{taskInfo.BackupStorageInfo.Path + str.FileName} ilgili .narbd uzantılı dosya bulunamadığı için silme işlemi yapılamıyor.");
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "{dizin} dizinindeki dosya silme işlemi başarısız.", taskInfo.BackupStorageInfo.Path + str.FileName);
            }
            try
            {
                if (File.Exists(Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName))
                    File.Delete(Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName);
                else
                    _logger.Information($"{Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName} ilgili .narmd uzantılı dosya bulunamadığı için silme işlemi yapılamıyor.");
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "{dizin} dizinindeki metadata dosyasını silme işlemi başarısız.", (Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName));
            }
        }

        private bool CheckDiskSize(TaskInfo taskInfo, StatusInfo statusInfo, Stopwatch timeElapsed, StreamInfo str, long BytesReadSoFar)
        {
            List<DiskInformation> diskList = GetDiskList();
            foreach (var diskItem in diskList)
            {
                foreach (var volumeItem in diskItem.VolumeInfos)
                {
                    if (taskInfo.BackupStorageInfo.Type == BackupStorageType.Windows &&
                        taskInfo.BackupStorageInfo.Path[0] == volumeItem.Letter &&
                        volumeItem.FreeSize < ((long)str.CopySize + 2147483648))
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
            _logger.Information("CancelTask metodu çağırıldı");
            var timeElapsed = _timeElapsedMap[taskInfo.Id];
            _cancellationTokenSource[taskInfo.Id].Cancel();
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
            List<FilesInBackup> filesInBackupList = new List<FilesInBackup>();
            try
            {
                var resultList = _cSNarFileExplorer.CW_GetFilesInCurrentDirectory();
                foreach (var item in resultList)
                {
                    FilesInBackup filesInBackup = new FilesInBackup();
                    filesInBackup.Id = item.UniqueID;
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
                    _logger.Debug($"{filesInBackup.Name} : {filesInBackup.Id}");
                }
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "GetFileInfoList() gerçekleştirilemedi.");
            }

            return filesInBackupList;
        }

        public void FreeFileExplorer()
        {
            //ilk çağırdığımızda init ettiğimiz FileExplorer metodunu kapatmak/serbest bırakmak için
            _logger.Verbose("FreeFileExplorer metodu çağırıldı");
            try
            {
                _cSNarFileExplorer.CW_Free();
                if (_nc != null)
                    _nc.Dispose();
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "_cSNarFileExplorer.CW_Free() gerçekleştirilemedi.");
            }
        }

        public bool GetSelectedFileInfo(ulong filesInBackupId)
        {
            _logger.Verbose("GetSelectedFileInfo metodu çağırıldı");
            try
            {
                var result = _cSNarFileExplorer.CW_SelectDirectory(filesInBackupId);
                _logger.Information($"_cSNarFileExplorer.CW_SelectDirectory({filesInBackupId}) işlemi sonucu: {result}");
                return result;
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "_cSNarFileExplorer.CW_SelectDirectory() gerçekleştirilemedi.");
            }
            return false;
        }

        public void PopDirectory()
        {
            _logger.Verbose("PopDirectory metodu çağırıldı");
            try
            {
                _cSNarFileExplorer.CW_PopDirectory();
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "_cSNarFileExplorer.CW_PopDirectory() gerçekleştirilemedi.");
            }
        }

        public string GetCurrentDirectory()
        {
            _logger.Verbose("GetCurrentDirectory metodu çağırıldı");
            try
            {
                return _cSNarFileExplorer.CW_GetCurrentDirectoryString();
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "_cSNarFileExplorer.CW_GetCurrentDirectoryString() gerçekleştirilemedi.");
            }
            return "";
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

        public FileRestoreResult RestoreFilesInBackup(FilesInBackup filesInBackup, string targetDirectory) // batuhan hangi backup olduğunu nasıl anlayacak? backup directoryde backup ismi almıyor
        {
            _logger.Verbose("RestoreFilesInBackup metodu çağırıldı");
            _logger.Information($"{filesInBackup.Name} isimli, {filesInBackup.Id} id'li dosya için indirme işlemi {targetDirectory} dizinine başlatılıyor");
            try
            {
                var sizeOfRestoredFilesConfiguration = _configurationDataDal.Get(x => x.Key == "sizeOfRestoredFiles");
                var sizeOfRestoredFiles = Convert.ToInt64(sizeOfRestoredFilesConfiguration.Value);
                Stopwatch passingTime = new Stopwatch();
                passingTime.Start();

                CSNarFileExportStream Stream = new CSNarFileExportStream(_cSNarFileExplorer, filesInBackup.Id);

                if (Stream.IsInit())
                {
                    FileStream file = File.Create(targetDirectory + filesInBackup.Name); // başına kullanıcının seçtiği path gelecek {targetPath + @"\" + SelectedEntry.Name}
                    unsafe
                    {
                        ulong buffersize = 1024 * 1024 * 64;
                        byte[] buffer = new byte[buffersize];
                        fixed (byte* baddr = &buffer[0])
                        {
                            while (Stream.AdvanceStream(baddr, buffersize))
                            {
                                file.Seek((long)Stream.TargetWriteOffset, SeekOrigin.Begin);
                                file.Write(buffer, 0, (int)Stream.TargetWriteSize);
                                sizeOfRestoredFiles += (int)Stream.TargetWriteSize;
                                if (passingTime.ElapsedMilliseconds > 500)
                                {
                                    sizeOfRestoredFilesConfiguration.Value = sizeOfRestoredFiles.ToString();
                                    _configurationDataDal.Update(sizeOfRestoredFilesConfiguration);
                                    passingTime.Restart();
                                }
                            }
                            sizeOfRestoredFilesConfiguration.Value = sizeOfRestoredFiles.ToString();
                            _configurationDataDal.Update(sizeOfRestoredFilesConfiguration);
                            file.SetLength((long)Stream.TargetFileSize);
                            file.Close();

                            if (Stream.Error != FileRestore_Errors.Error_NoError)
                            {
                                return FileRestoreResult.RestoreFailed;
                            }
                            else if (Stream.Error == FileRestore_Errors.Error_NoError)
                            {
                                return FileRestoreResult.RestoreSuccessful;
                            }
                        }
                    }
                }
                else
                {
                    return FileRestoreResult.RestoreFailedToStart;
                }
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "_cSNarFileExplorer.CW_RestoreFile() gerçekleştirilemedi.");
            }
            return FileRestoreResult.Fail;
        }

        private BackupInfo ConvertToBackupInfo(BackupStorageInfo backupStorageItem, BackupMetadata backupMetadata)
        {
            BackupInfo backupInfo = new BackupInfo();
            backupInfo.Letter = backupMetadata.Letter;
            backupInfo.Version = backupMetadata.Version;
            backupInfo.OSVolume = backupMetadata.OSVolume;
            backupInfo.DiskType = backupMetadata.DiskType; //mbr gpt
            backupInfo.OS = backupMetadata.WindowsName;
            backupInfo.BackupTaskName = backupMetadata.TaskName;
            backupInfo.Description = backupMetadata.TaskDescription;
            backupInfo.BackupStorageInfo = backupStorageItem;
            backupInfo.BackupStorageInfoId = backupStorageItem.Id;
            backupInfo.Bootable = Convert.ToBoolean(backupMetadata.OSVolume);
            backupInfo.VolumeSize = (long)backupMetadata.VolumeTotalSize;
            backupInfo.StrVolumeSize = FormatBytes((long)backupMetadata.VolumeTotalSize);
            backupInfo.UsedSize = (long)backupMetadata.VolumeUsedSize;
            backupInfo.StrUsedSize = FormatBytes((long)backupMetadata.VolumeUsedSize);
            backupInfo.FileSize = (long)backupMetadata.BytesNeedToCopy;
            backupInfo.StrFileSize = FormatBytes((long)backupMetadata.BytesNeedToCopy);
            backupInfo.FileName = backupMetadata.Fullpath.Split('\\').Last();
            backupInfo.PCName = backupMetadata.ComputerName;
            backupInfo.IpAddress = backupMetadata.IpAdress;
            string createdDate = (backupMetadata.BackupDate.Day < 10) ? 0 + backupMetadata.BackupDate.Day.ToString() : backupMetadata.BackupDate.Day.ToString();
            createdDate = createdDate + "." + ((backupMetadata.BackupDate.Month < 10) ? 0 + backupMetadata.BackupDate.Month.ToString() : backupMetadata.BackupDate.Month.ToString());
            createdDate = createdDate + "." + backupMetadata.BackupDate.Year + " ";
            createdDate = createdDate + ((backupMetadata.BackupDate.Hour < 10) ? 0 + backupMetadata.BackupDate.Hour.ToString() : backupMetadata.BackupDate.Hour.ToString());
            createdDate = createdDate + ":" + ((backupMetadata.BackupDate.Minute < 10) ? 0 + backupMetadata.BackupDate.Minute.ToString() : backupMetadata.BackupDate.Minute.ToString());
            createdDate = createdDate + ":" + ((backupMetadata.BackupDate.Second < 10) ? 0 + backupMetadata.BackupDate.Second.ToString() : backupMetadata.BackupDate.Second.ToString());
            backupInfo.CreatedDate = Convert.ToDateTime(createdDate, CultureInfo.CreateSpecificCulture("tr-TR")); ;
            backupInfo.MetadataFileName = backupMetadata.Metadataname;

            if (backupMetadata.Version == -1)
                backupInfo.Type = BackupTypes.Full;
            else
                backupInfo.Type = (BackupTypes)backupMetadata.BackupType; // 2 full - 1 inc - 0 diff - BATU' inc 1 - diff 0
            return backupInfo;
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

        /*0 - NAS'a bağlanamama
          1 - Silme işlemleri gerçekleştirilemedi
          2 - Sorun yaşanmadı veya silinecek dosya bulunmadı (zincirden silebileceğimiz için direkt kullanıcıyı bilgilendirmiyoruz)*/
        public byte BackupFileDelete(BackupInfo backupInfo)
        {
            var backupStorageInfo = _backupStorageDal.Get(x => x.Id == backupInfo.BackupStorageInfoId);

            NetworkConnection nc = null;
            if (backupStorageInfo.Type == BackupStorageType.NAS)
            {
                nc = NASConnection(backupStorageInfo);
                if (nc == null)
                    return 0; // bağlantı sağlanamadığı için işlem gerçekleştirilemiyor
            }

            BackupMetadata backupMetadata = GetBackupMetadata(backupInfo, backupStorageInfo);

            if (nc != null)
                nc.Dispose();

            if (backupMetadata.Version != -200) // ilgili metadata bulundu
            {
                //tüm backupları getir
                var backupStorageList = _backupStorageDal.GetList();
                List<BackupMetadata> backupMetadataAllList = new List<BackupMetadata>();
                int index = 0;
                foreach (BackupStorageInfo backupStorageItem in backupStorageList)
                {
                    if (!IsItTheSamePath(backupStorageList, index, backupStorageItem))
                    {
                        NetworkConnection nc2 = null;
                        if (backupStorageItem.Type == BackupStorageType.NAS)
                        {
                            nc2 = NASConnection(backupStorageItem);
                        }

                        var returnList = DiskTracker.CW_GetBackupsInDirectory(backupStorageItem.Path);
                        foreach (var returnItem in returnList)
                        {
                            backupMetadataAllList.Add(returnItem);
                        }

                        if (nc2 != null)
                            nc2.Dispose();
                    }
                    index++;
                }

                // zinciri elde et
                List<BackupMetadata> chainList = GetChainList(backupMetadata, backupMetadataAllList);

                if (backupMetadata.BackupType == 0) // 0 diff
                {
                    if (backupMetadata.Version == -1)
                    {
                        //tüm zinciri sil
                        foreach (var item in chainList)
                        {
                            var result = DeleteBackupFileAndMetadata(backupStorageInfo, item, backupStorageList);
                            if (result != 2)
                                return result;
                        }

                        if (backupMetadata.IsSameChainID(_diskTracker.CW_IsVolumeExists(backupMetadata.Letter)))
                        {
                            CleanChain(backupMetadata.Letter);
                            _logger.Information($"{backupMetadata.Fullpath} silinirken '{backupMetadata.Letter}' volume'nde işleyen zincir olduğu için yeni zincir başlatıldı.");
                        }
                    }
                    else
                    {
                        // direkt sil etkilenecek backup yok
                        var result = DeleteBackupFileAndMetadata(backupStorageInfo, backupMetadata, backupStorageList);
                        if (result != 2)
                            return result;
                    }
                }
                else if (backupMetadata.BackupType == 1) // 1 inc
                {
                    int index2 = 0;
                    foreach (var backupMetadataItem in chainList)
                    {
                        if (backupMetadataItem.Version == backupMetadata.Version)
                        {
                            for (int i = index2; i < chainList.Count; i++)
                            {
                                var result = DeleteBackupFileAndMetadata(backupStorageInfo, chainList[i], backupStorageList);
                                if (result != 2)
                                    return result;
                            }
                            break;
                        }
                        index2++;
                    }

                    if (backupMetadata.IsSameChainID(_diskTracker.CW_IsVolumeExists(backupMetadata.Letter)))
                    {
                        CleanChain(backupMetadata.Letter);
                        _logger.Information($"{backupMetadata.Fullpath} silinirken '{backupMetadata.Letter}' volume'nde işleyen zincir olduğu için yeni zincir başlatıldı.");
                    }
                }
            }

            return 2;
        }

        private byte DeleteBackupFileAndMetadata(BackupStorageInfo backupStorageInfo, BackupMetadata backupMetadata, List<BackupStorageInfo> backupStorageList)
        {
            NetworkConnection nc2 = null;
            string backupName = backupMetadata.Fullpath.Split('\\').Last();
            string newRootDir = backupMetadata.Fullpath.Substring(0, backupMetadata.Fullpath.Length - backupName.Length);
            if (newRootDir[0].Equals('\\')) // nasdır connection aç
            {
                foreach (var backupStorageItem in backupStorageList)
                {
                    if (newRootDir.Equals(backupStorageItem.Path) && backupStorageInfo.Type == BackupStorageType.NAS)
                    {
                        nc2 = NASConnection(backupStorageInfo);
                        if (nc2 == null)
                        {
                            _logger.Error("Uzak paylaşıma bağlanılamadığı için backup dosyası silinemiyor. {path}", backupMetadata.Fullpath);
                            return 0; // bağlantı sağlanamadığı için işlem gerçekleştirilemiyor
                        }
                        break;
                    }
                }
            }

            try
            {
                File.Delete(backupMetadata.Fullpath); //.narbd
                File.Delete(newRootDir + backupMetadata.Metadataname); //.narmd
            }
            catch (IOException ex)
            {
                _logger.Error(ex, "{dizin} dizinindeki dosya silme işlemi başarısız.", backupMetadata.Fullpath);
                return 1; // silme işleminde sorun çıktı
            }

            if (nc2 != null)
                nc2.Dispose();

            return 2;
        }

        private BackupMetadata GetBackupMetadata(BackupInfo backupInfo, BackupStorageInfo backupStorageInfo)
        {
            var backupMetadataList = DiskTracker.CW_GetBackupsInDirectory(backupStorageInfo.Path);
            BackupMetadata backupMetadata = new BackupMetadata { Version = -200 };
            foreach (var backupMetadataItem in backupMetadataList)
            {
                if (backupMetadataItem.Fullpath == backupInfo.BackupStorageInfo.Path + backupInfo.FileName)
                    backupMetadata = backupMetadataItem;
            }

            return backupMetadata;
        }

        private List<BackupMetadata> GetChainList(BackupMetadata backupMetadata, List<BackupMetadata> backupMetadataAllList)
        {
            List<BackupMetadata> chainList = new List<BackupMetadata>();
            foreach (var itemBackupMetadata in backupMetadataAllList)
            {
                if (!itemBackupMetadata.IsSameChainID(backupMetadata))
                    chainList.Add(itemBackupMetadata);
            }

            chainList = chainList.OrderBy(x => x.Version).ToList(); // versionlara göre küçükten büyüğe sıralama işlemi yapıyoruz
            return chainList;
        }

        private bool IsItTheSamePath(List<BackupStorageInfo> backupStorageList, int index, BackupStorageInfo backupStorageItem)
        {
            for (int i = index - 1; i >= 0; i--)
            {
                if (backupStorageItem.Path == backupStorageList[i].Path)
                {
                    return true;
                }
            }
            return false;
        }

        private NetworkConnection NASConnection(BackupStorageInfo backupStorageInfo)
        {
            NetworkConnection nc = null;
            try
            {
                return new NetworkConnection(backupStorageInfo.Path.Substring(0, backupStorageInfo.Path.Length - 1), backupStorageInfo.Username, backupStorageInfo.Password, backupStorageInfo.Domain);
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Uzak paylaşıma bağlanılamadığı için backup dosyalarına erişilemiyor. {path}", backupStorageInfo.Path);
                return nc;
            }
        }

    }
}
