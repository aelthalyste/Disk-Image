using System;
using System.Collections.Generic;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.Entities.Concrete;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace DiskBackup.DataAccess.Tests.EntityFramework
{
    [TestClass]
    public class EfBackupStorageTest
    {
        EfBackupStorageDal _EfBackupStorageDal = new EfBackupStorageDal();

        [TestMethod]
        public void AddBackupStorageTest()
        {
            var backupStorage = new BackupStorageInfo
            {
                StorageName = "AddBackupStorageTest 4",
                Type = (BackupStorageType)1,
                Description = "AddBackupStorageTest 4",
                Path = "C:\\Users\\ebruv\\source\\repos\\Disk-Image\\DiskBackup.DataAccess.Tests\\Sample",
                IsCloud = false,
                Domain = null,
                Username = "denemeUser3",
                Password = "Parola123",
            };
            var result = _EfBackupStorageDal.Add(backupStorage);
            Assert.AreEqual(result, backupStorage);
        }

        [TestMethod]
        public void GetStorageTest()
        {
            var result = _EfBackupStorageDal.Get(x => x.Id == 7);
            Assert.AreEqual(result.Path, "C:\\Users\\ebruv\\source\\repos\\Disk-Image\\DiskBackup.DataAccess.Tests\\Sample");
        }

        [TestMethod]
        public void GetListStorageTest()
        {
            var backupStorage = new BackupStorageInfo
            {
                StorageName = "Deneme 2",
                Type = (BackupStorageType)1,
                Description = "Açıklama 2",
                Path = "C:\\Users\\ebruv\\source\\repos\\Disk-Image\\DiskBackup.DataAccess.Tests\\Sample",
                IsCloud = false,
                Domain = null,
                Username = "denemeUser2",
                Password = "Parola123",
            };
            _EfBackupStorageDal.Add(backupStorage);

            var result = _EfBackupStorageDal.GetList();
            Assert.IsTrue(result.Count > 0);
        }

        [TestMethod]
        public void DeleteStorageTest()
        {
            var backupStorage = new BackupStorageInfo
            {
                Id = 454545
            };
            _EfBackupStorageDal.Add(backupStorage);

            _EfBackupStorageDal.Delete(backupStorage);
        }

        [TestMethod]
        public void DeleteListStorageTest()
        {
            List<BackupStorageInfo> backupStorageList = new List<BackupStorageInfo>();
            var backupStorage = new BackupStorageInfo
            {
                Id = 454545
            };
            var result = _EfBackupStorageDal.Add(backupStorage);
            backupStorageList.Add(result);
            Assert.IsNotNull(backupStorage);

            backupStorage = new BackupStorageInfo
            {
                Id = 454546
            };
            _EfBackupStorageDal.Add(backupStorage);
            backupStorageList.Add(backupStorage);
            Assert.IsNotNull(result);

            _EfBackupStorageDal.Delete(backupStorageList);
            Assert.IsNull(_EfBackupStorageDal.Get(x => x.Id == 454546));
        }

        [TestMethod]
        public void UpdateStorageTest()
        {
            var result = _EfBackupStorageDal.Get(x => x.Id == 2);
            Assert.IsNotNull(result);
            result.StorageName = "Değiştirildi 1";
            _EfBackupStorageDal.Update(result);
            result = _EfBackupStorageDal.Get(x => x.Id == 2);
            Assert.IsNotNull(result);
            Assert.AreEqual(result.StorageName, "Değiştirildi 1");
        }
    }
}
