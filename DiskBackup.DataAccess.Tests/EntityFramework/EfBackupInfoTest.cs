using System;
using System.Collections.Generic;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.Entities.Concrete;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace DiskBackup.DataAccess.Tests.EntityFramework
{
    [TestClass]
    public class EfBackupInfoTest
    {
        //EfBackupInfoDal _EfBackupInfoDal = new EfBackupInfoDal();

        //[TestMethod]
        //public void AddBackupInfo()
        //{
        //    var backupInfo = new BackupInfo
        //    {
        //        BackupStorageInfoId = 2,
        //        Type = (BackupTypes)1,
        //        FileName = "Volume_C_Full 1",
        //        CreatedDate = DateTime.Now,
        //        BackupTaskName = "C Full",
        //        VolumeSize = 9999999,
        //        StrVolumeSize = "90 GB",
        //        FileSize = 111,
        //        StrFileSize = "11 GB",
        //        Description = "Açıklama 1",
        //        IsCloud = false
        //    };

        //    var result = _EfBackupInfoDal.Add(backupInfo);
        //    Assert.AreEqual(result, backupInfo);
        //}

        //[TestMethod]
        //public void AddListBackupInfo()
        //{
        //    List<BackupInfo> backupInfoList = new List<BackupInfo>();

        //    var backupInfo = new BackupInfo
        //    {
        //        BackupStorageInfoId = 2,
        //        Type = (BackupTypes)1,
        //        FileName = "Volume_C_Full 2",
        //        CreatedDate = DateTime.Now,
        //        BackupTaskName = "C Full",
        //        VolumeSize = 9999999,
        //        StrVolumeSize = "90 GB",
        //        FileSize = 111,
        //        StrFileSize = "11 GB",
        //        Description = "Açıklama 2",
        //        IsCloud = false
        //    };
        //    backupInfoList.Add(backupInfo);
        //    Assert.IsNotNull(backupInfo);

        //    backupInfo = new BackupInfo
        //    {
        //        BackupStorageInfoId = 2,
        //        Type = (BackupTypes)1,
        //        FileName = "Volume_C_Full 5",
        //        CreatedDate = DateTime.Now,
        //        BackupTaskName = "C Full",
        //        VolumeSize = 9999999,
        //        StrVolumeSize = "90 GB",
        //        FileSize = 111,
        //        StrFileSize = "11 GB",
        //        Description = "Açıklama 5",
        //        IsCloud = false
        //    };
        //    backupInfoList.Add(backupInfo);
        //    Assert.IsNotNull(backupInfo);

        //    _EfBackupInfoDal.Add(backupInfoList);
        //    Assert.AreEqual(_EfBackupInfoDal.Get(x => x.FileName == "Volume_C_Full 5").FileName, "Volume_C_Full 5");
        //    _EfBackupInfoDal.Delete(_EfBackupInfoDal.Get(x => x.FileName == "Volume_C_Full 5"));
        //}

        //[TestMethod]
        //public void DeleteBackupInfo()
        //{
        //    var backupInfo = new BackupInfo
        //    {
        //        Id = 999999999
        //    };

        //    var result = _EfBackupInfoDal.Add(backupInfo);
        //    _EfBackupInfoDal.Delete(result);
        //    Assert.IsNull(_EfBackupInfoDal.Get(x => x.Id == 999999999));
        //}

        //[TestMethod]
        //public void DeleteListStorageTest()
        //{
        //    List<BackupInfo> backupInfoList = new List<BackupInfo>();
        //    var backupInfo = new BackupInfo
        //    {
        //        Id = 454545
        //    };
        //    var result = _EfBackupInfoDal.Add(backupInfo);
        //    backupInfoList.Add(result);
        //    Assert.IsNotNull(backupInfo);

        //    backupInfo = new BackupInfo
        //    {
        //        Id = 454546
        //    };
        //    _EfBackupInfoDal.Add(backupInfo);
        //    backupInfoList.Add(backupInfo);
        //    Assert.IsNotNull(result);

        //    _EfBackupInfoDal.Delete(backupInfoList);
        //    Assert.IsNull(_EfBackupInfoDal.Get(x => x.Id == 454546));
        //}

        //[TestMethod]
        //public void GetBackupInfoTest()
        //{
        //    var result = _EfBackupInfoDal.Get(x => x.Id == 4);
        //    Assert.AreEqual(result.FileName, "Volume_D_Diff 1");

        //}

        //[TestMethod]
        //public void GetListBackupInfoTest()
        //{
        //    var backupInfo = new BackupInfo
        //    {
        //        BackupStorageInfoId = 2,
        //        Type = (BackupTypes)2,
        //        FileName = "Volume_D_Inc 2",
        //        CreatedDate = DateTime.Now,
        //        BackupTaskName = "C Inc",
        //        VolumeSize = 9999999,
        //        StrVolumeSize = "90 GB",
        //        FileSize = 111,
        //        StrFileSize = "11 GB",
        //        Description = "Açıklama deneme 2",
        //        IsCloud = false,
        //        Version = 1
        //    };
        //    _EfBackupInfoDal.Add(backupInfo);

        //    var result = _EfBackupInfoDal.GetList();
        //    Assert.IsTrue(result.Count > 0);
        //}
    }
}
