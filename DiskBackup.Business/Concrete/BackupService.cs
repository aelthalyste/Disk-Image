using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.DataAccess.Core;
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
    public class BackupService : IBackupService
    {
        private DiskTracker _diskTracker = new DiskTracker();
        private CSNarFileExplorer _cSNarFileExplorer = new CSNarFileExplorer();

        private Dictionary<int, ManualResetEvent> _taskEventMap = new Dictionary<int, ManualResetEvent>(); // aynı işlem Stopwatch ve cancellationtoken source için de yapılacak ancak Quartz'ın pause, resume ve cancel işlemleri düzgün çalışıyorsa kullanılmayacak
        private Dictionary<int, CancellationTokenSource> _cancellationTokenSource = new Dictionary<int, CancellationTokenSource>();
        private bool _isStarted = false;

        private Dictionary<int, Stopwatch> _timeElapsedMap = new Dictionary<int, Stopwatch>();

        private readonly IStatusInfoDal _statusInfoDal; // status bilgilerini veritabanına yazabilmek için gerekli
        private readonly ITaskInfoDal _taskInfoDal; // status bilgilerini veritabanına yazabilmek için gerekli

        public BackupService(IStatusInfoDal statusInfoRepository, ITaskInfoDal taskInfoDal)
        {
            _statusInfoDal = statusInfoRepository;
            _taskInfoDal = taskInfoDal;
        }

        public bool InitTracker()
        {
            // programla başlat 1 kere çalışması yeter
            // false değeri dönüyor ise eğer backup işlemlerini disable et
            return _diskTracker.CW_InitTracker();
        }

        public void InitFileExplorer(BackupInfo backupInfo) //initTracker'la aynı mantıkla çalışır mı? (Explorer ctor'da 1 kere çağrılma)
        {
            //rootDir string biz buraya ne dönücez
            // yedek volumu, versiondan gelecek, "E:\ebru-eyupDeneme"-- ters slaş ekle sonuna
            _cSNarFileExplorer.CW_Init('C', 0, "");
        }

        public List<DiskInformation> GetDiskList()
        {
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

                    if (returnItem.BackupType == -1)
                        backupInfo.Type = BackupTypes.Full;
                    else
                        backupInfo.Type = (BackupTypes)returnItem.BackupType; // BackupTypes enum = 2 full - 1 inc - 0 diff / BATU' inc 1 - diff 0

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

                    if (resultItem.BackupType == -1)
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
        }

        public bool RestoreBackupDisk(BackupInfo backupInfo, DiskInformation diskInformation)
        {
            //batudan fonksiyon gelecek o fonksiyon hangi harfle restore edeceğini dönecek ve batu o harfle restore edecek
            //DiskTracker.CW_GetFirstAvailableVolumeLetter(); Batuhandan müsait letter alınması
            //DiskTracker.CW_RestoreToFreshDisk(DiskTracker.CW_GetFirstAvailableVolumeLetter(), "backup harfi", "version bilgisi", "diskId", "backup'ın bulunduğu yol *\*");
            //pathde sadece path varmış dosya adı yokmuş
            //CW_GetFirstAvailableVolumeLetter ile boş olan volume'ü alıp batuhan'a dönerek o volume restore gerçekleştirilecek
            throw new NotImplementedException();
        }

        public bool CreateIncDiffBackup(TaskInfo taskInfo) // 0 başarısız, 1 başarılı, 2 kullanıcı durdurdu
        {
            Console.WriteLine("Batu Method girildi");
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

            foreach (var letter in letters) // C D E F
            {
                if (_diskTracker.CW_SetupStream(letter, (int)taskInfo.BackupTaskInfo.Type, str)) // 0 diff, 1 inc, full (2) ucu gelmediğinden ayrılabilir veya aynı devam edebilir
                {
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
                                    return false;
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
                            Console.WriteLine($"{BytesReadSoFar} okundu --- {str.CopySize} okunması gereken");

                            try
                            {
                                File.Copy(str.MetadataFileName, taskInfo.BackupStorageInfo.Path + str.MetadataFileName); //backupStorageInfo path alınıcak
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
                statusInfo.TimeElapsed = timeElapsed.ElapsedMilliseconds;
                _statusInfoDal.Update(statusInfo);
            }
            manualResetEvent.Dispose();
            _taskEventMap.Remove(taskInfo.Id);

            _timeElapsedMap.Remove(taskInfo.Id);

            _cancellationTokenSource[taskInfo.Id].Dispose();
            _cancellationTokenSource.Remove(taskInfo.Id);
            Console.WriteLine("Batu Method çıktı");
            return result;
        }

        public bool CreateFullBackup(TaskInfo taskInfo) //bu method daha gelmedi 
        {
            throw new NotImplementedException();
        }

        public void PauseTask(TaskInfo taskInfo)
        {
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
            taskInfo.Status = "Durduruldu";
            _taskInfoDal.Update(taskInfo);
        }

        public void CancelTask(TaskInfo taskInfo)
        {
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
            taskInfo.Status = "Hazır"; 
            _taskInfoDal.Update(taskInfo);
        }

        public void ResumeTask(TaskInfo taskInfo)
        {
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
            taskInfo.Status = "Çalışıyor";
            _taskInfoDal.Update(taskInfo);
        }

        public List<FilesInBackup> GetFileInfoList()
        {
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

        public void RestoreFilesInBackup(int fileId, string backupDirectory, string targetDirectory) // batuhan hangi backup olduğunu nasıl anlayacak? backup directoryde backup ismi almıyor
        {
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
