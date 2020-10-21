﻿using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Abstract
{
    public interface IBackupService
    {
        List<DiskInformation> GetDiskList(); //Systemdeki disklerin görüntülenmesi için Batuhanın ucu var (CW_DisksOnSystem) & (CW_GetVolumes' pcName, ipAddres vs eklenecekmiş)
        List<BackupInfo> GetBackupFileList(List<BackupStorageInfo> backupStorageList);  //parametre olarak path alıyor -> dönüş değeri liste (CW_GetBackupsInDirectory)
        BackupInfo GetBackupFile(BackupInfo backupInfo); // istediğimiz backup bilgilerini almak için
        List<FilesInBackup> GetFileInfoList(); //Ucu belli değil 
        List<Log> GetLogList(); //Böyle bir uç yapılacağı konuşuldu

        bool PauseTask(TaskInfo taskInfo);
        bool CancelTask(TaskInfo taskInfo);
        bool ResumeTask(TaskInfo taskInfo);
        
        //INC ve DIFF TaskInfo'dan strObjeyi alarak çoklu seçim yapabilir
        bool CreateIncrementalBackup(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo);
        bool CreateDifferentialBackup(TaskInfo taskInfo, BackupStorageInfo backupStorageInfo);
        bool CreateFullBackup(TaskInfo taskInfo); //Bu daha hazır değil        
        
        bool RestoreBackupVolume(BackupInfo backupInfo, VolumeInfo volumeInfo); //Parametreler bu methodun içinde RestoreTask oluşturacak
        bool RestoreBackupDisk(BackupInfo backupInfo, DiskInformation diskInformation);
        //Restore işleminde disk seçilirse CW_RestoreToFreshDisk, volume seçilirse CW_RestoreToVolume
        //CW_RestoreToVolume(char TargetLetter, char SrcLetter, int Version, bool ShouldFormat, string RootDir) 
        //CW_RestoreToFreshDisk(char TargetLetter, char SrcLetter, int Version, int DiskID, string Rootdir) methodları ayrı restore işlemi yapan methodlar

        //uçlar belli değil
        bool RestoreFile(BackupInfo backupInfo, FilesInBackup fileInfo, string destination);
        bool RestoreFolder(BackupInfo backupInfo, FilesInBackup fileInfo, string destination);
        //...
        bool InitTracker(); //CW_InitTracker driver okumayla ilgili bir method

        //bool AddBackupArea(BackupStorageInfo backupArea); Bu veriyi de biz oluşturucaz
        //List<BackupStorageInfo> GetBackupAreaInfoList(); Bu veriyi biz tutucaz uçlara biz göndericez

        //File Backup Browse methodları eklenebilir tam bilinmiyor
        //Yüzde hesabı Read Write' a göre yapılacak
        //Okuma olmadığında pause olucak
        //TerminateBackup'a false verilince durdurulacak
        //StreamInfo class işlemleri (ReadStream, SetupStream) ?? (status progressbarlar doldurulur)
    }
}