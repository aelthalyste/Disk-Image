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

        public void InitFileExplorer(BackupInfo backupInfo)
        {
            _logger.Verbose("InitFileExplorer metodu çağırıldı");
            _logger.Verbose("İnitFileExplorer: {path}, {name}", backupInfo.BackupStorageInfo.Path, backupInfo.MetadataFileName);
            _cSNarFileExplorer.CW_Init(backupInfo.BackupStorageInfo.Path, backupInfo.MetadataFileName); // isim eklenmesi gerekmeli gibi
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
            //usedSize, bootable, sıkıştırma, pc name, ip address
            _logger.Verbose("GetBackupFileList metodu çağırıldı");
            List<BackupInfo> backupInfoList = new List<BackupInfo>();
            //bootable = osVolume (true)
            foreach (BackupStorageInfo backupStorageItem in backupStorageList)
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

            string backupName = taskInfo.RestoreTaskInfo.RootDir.Split('\\').Last();
            string newRootDir = taskInfo.RestoreTaskInfo.RootDir.Substring(0, taskInfo.RestoreTaskInfo.RootDir.Length - backupName.Length);
            var resultList = DiskTracker.CW_GetBackupsInDirectory(newRootDir);
            BackupMetadata backupMetadata = new BackupMetadata();

            foreach (var item in resultList)
            {
                if (item.Fullpath.Equals(taskInfo.RestoreTaskInfo.RootDir))
                {
                    backupMetadata = item;
                    _logger.Verbose("|{@restoreTaskId}| restore taskı için restoreVolume gerçekleştirilecek.", taskInfo.RestoreTaskInfo.Id);

                    if (ChainInTheSameDirectory(resultList, backupMetadata))
                        return Convert.ToByte(DiskTracker.CW_RestoreToVolume(taskInfo.RestoreTaskInfo.TargetLetter[0], backupMetadata, true, newRootDir)); //true gidecek
                    else
                        return 3; // eksik dosya hatası yazdır
                }
            }

            if (nc != null)
                nc.Dispose();

            _logger.Verbose("|{@restoreTaskId}| restore taskı için backupMetadata bulunamadı.", taskInfo.RestoreTaskInfo.Id);
            return 0;
        }

        private bool ChainInTheSameDirectory(List<BackupMetadata> backupMetadataList, BackupMetadata backupMetadata)
        {
            // Tüm dosyalar aynı dizinde mi kontrolü yap
            List<BackupMetadata> chainList = new List<BackupMetadata>();

            if (backupMetadata.Version != -1)
            {
                _logger.Information("Backup Version: " + backupMetadata.Version.ToString());
                _logger.Information("Backup Type: {type} | 0 diff - 1 inc", backupMetadata.BackupType.ToString());
                foreach (var itemBackupMetadata in backupMetadataList)
                {
                    if (!itemBackupMetadata.IsSameChainID(backupMetadata))
                        chainList.Add(itemBackupMetadata);
                }

                chainList = chainList.OrderBy(x => x.Version).ToList();
                _logger.Information("----------------------------------LİSTE--------------------------");
                chainList.ForEach(x => _logger.Information(x.Version.ToString() + " - " + x.Fullpath + " - " + x.BackupType.ToString()));
                _logger.Information("--------------------------------------------------------------------------------------------------------");

                if (backupMetadata.BackupType == 0) // 0 diff - diff restore
                {
                    // Full ve restore edilecek backup aynı dizinde olması gerekiyor.
                    if (chainList[0].Version == -1)
                    {
                        _logger.Information("if (chainList[0].Version == -1) TRUE");
                        return true;
                    }                      
                    else
                    {
                        _logger.Information("if (chainList[0].Version == -1) FALSE");
                        return false;
                    }
                                           
                }
                else if (backupMetadata.BackupType == 1) // 1 inc - inc restore
                {
                    // ilgili backup versiyonuna kadar tüm backuplar aynı dizinde olmalı. Sayıyı kontrol et, versiyonları kontrol et
                    _logger.Information("Count: {c} - Version: {v}", chainList.Count, backupMetadata.Version + 2);
                    if (chainList.Count >= backupMetadata.Version + 2)
                    {                      
                        for (int i = 0; i <= backupMetadata.Version + 1; i++)
                        {
                            if (!(chainList[i].Version == i - 1))
                            {
                                _logger.Information("Bu backup gerçekleştirilemez");
                                return false;
                            }                               
                        }
                        return true;
                    }
                    else
                        return false;
                }
            }
            // zaten zincirin devamına bakmaya gerek yok
            return true;
        }

        public byte RestoreBackupDisk(TaskInfo taskInfo)
        {
            _logger.Verbose("RestoreBackupDisk metodu çağırıldı");
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

            string backupName = taskInfo.RestoreTaskInfo.RootDir.Split('\\').Last();
            string newRootDir = taskInfo.RestoreTaskInfo.RootDir.Substring(0, taskInfo.RestoreTaskInfo.RootDir.Length - backupName.Length);
            var resultList = DiskTracker.CW_GetBackupsInDirectory(newRootDir);
            BackupMetadata backupMetadata = new BackupMetadata();

            foreach (var item in resultList)
            {
                if (item.Fullpath.Equals(taskInfo.RestoreTaskInfo.RootDir))
                {
                    backupMetadata = item;
                    _logger.Verbose("|{@restoreTaskId}| restore taskı için restoreDisk gerçekleştirilecek.", taskInfo.RestoreTaskInfo.Id);

                    if (ChainInTheSameDirectory(resultList, backupMetadata))
                        return Convert.ToByte(DiskTracker.CW_RestoreToFreshDisk(taskInfo.RestoreTaskInfo.TargetLetter[0], backupMetadata, taskInfo.RestoreTaskInfo.DiskId, newRootDir));
                    else
                        return 3; // eksik dosya hatası yazdır
                }
            }

            if (nc != null)
                nc.Dispose();

            _logger.Verbose("|{@restoreTaskId}| restore taskı için backupMetadata bulunamadı.", taskInfo.RestoreTaskInfo.Id);
            return 0;
        }

        public bool CleanChain(char letter)
        {
            _logger.Verbose("CleanChain metodu çağırıldı");
            _logger.Information("{letter} zinciri temizleniyor.", letter);
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

            foreach (var letter in letters) // C D E F
            {
                _logger.Information($"{letter} backup işlemi başlatılıyor...");
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
                                    DeleteMissingBackupFile(taskInfo, str, file);
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
            DeleteBrokenBackupFiles(taskInfo, str);
            return 0;
        }

        private void DeleteBrokenBackupFiles(TaskInfo taskInfo, StreamInfo str)
        {
            _logger.Information("Görev başarısız olduğu için {dizin} siliniyor.", taskInfo.BackupStorageInfo.Path + str.FileName);
            try
            {
                File.Delete(taskInfo.BackupStorageInfo.Path + str.FileName);
            }
            catch (IOException ex)
            {
                _logger.Error(ex, "{dizin} dizinindeki dosya silme işlemi başarısız.", taskInfo.BackupStorageInfo.Path + str.FileName);
            }
            _logger.Information("Görev başarısız olduğu için {dizin} siliniyor.", (Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName));
            try
            {
                File.Delete(Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName);
            }
            catch (IOException ex)
            {
                _logger.Error(ex, "{dizin} dizinindeki metadata dosyasını silme işlemi başarısız.", (Directory.GetCurrentDirectory() + @"\" + str.MetadataFileName));
            }
        }

        private unsafe void DeleteMissingBackupFile(TaskInfo taskInfo, StreamInfo str, FileStream file)
        {
            // oluşturulan dosyayı sil
            _logger.Information("Görev iptal edildiği için {dizin} siliniyor.", taskInfo.BackupStorageInfo.Path + str.FileName);
            try
            {
                file.Close();
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "{dizin} dizinindeki dosyayı kapatma işlemi başarısız.", taskInfo.BackupStorageInfo.Path + str.FileName);
            }
            try
            {
                File.Delete(taskInfo.BackupStorageInfo.Path + str.FileName);
            }
            catch (IOException ex)
            {
                _logger.Error(ex, "{dizin} dizinindeki dosya silme işlemi başarısız.", taskInfo.BackupStorageInfo.Path + str.FileName);
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
            _logger.Information("CancelTask metodu çağırıldı");

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
            cSNarFileEntry.IsDirectory = Convert.ToBoolean(filesInBackup.Type);
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

        public void RestoreFilesInBackup(long fileId, string backupDirectory, string targetDirectory) // batuhan hangi backup olduğunu nasıl anlayacak? backup directoryde backup ismi almıyor
        {
            _logger.Verbose("RestoreFilesInBackup metodu çağırıldı");
            _cSNarFileExplorer.CW_RestoreFile(fileId, backupDirectory, targetDirectory);
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

    }
}
