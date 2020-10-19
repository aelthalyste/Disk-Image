using System;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.Entities.Concrete;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace DiskBackup.DataAccess.Tests.EntityFramework
{
    [TestClass]
    public class EfBackupStorageTest
    {
        [TestMethod]
        public void AddBackupStorageTest()
        {
            EfBackupStorageDal efBackupStorageDal = new EfBackupStorageDal();
            var backupStorage = new BackupStorageInfo
            {
                StorageName = "Deneme 1",
                Type = (BackupStorageType)1,
                Description = "Açıklama 1",
                Capacity = 123456789,
                UsedSize = 123456,
                FreeSize = 7890000,
                Path = "C:\\Users\\ebruv\\source\\repos\\Disk-Image\\DiskBackup.DataAccess.Tests\\Sample",
                IsCloud = false,
                Domain = null,
                Username = "denemeUser1",
                Password = "Parola123",
                StrCapacity = "123 GB",
                StrUsedSize = "100 GB",
                StrFreeSize = "23 GB"
            };
            var result = efBackupStorageDal.Add(backupStorage);
            Assert.AreEqual(backupStorage.Id, result.Id);
            Assert.AreEqual(backupStorage.StorageName, result.StorageName);
            Assert.AreEqual(backupStorage.Type, result.Type);
            Assert.AreEqual(backupStorage.Description, result.Description);
            Assert.AreEqual(backupStorage.Capacity, result.Capacity);
            Assert.AreEqual(backupStorage.UsedSize, result.UsedSize);
            Assert.AreEqual(backupStorage.FreeSize, result.FreeSize);
            Assert.AreEqual(backupStorage.Path, result.Path);
            Assert.AreEqual(backupStorage.IsCloud, result.IsCloud);
            Assert.AreEqual(backupStorage.Domain, result.Domain);
            Assert.AreEqual(backupStorage.Username, result.Username);
            Assert.AreEqual(backupStorage.Password, result.Password);
            Assert.AreEqual(backupStorage.StrCapacity, result.StrCapacity);
            Assert.AreEqual(backupStorage.StrUsedSize, result.StrUsedSize);
            Assert.AreEqual(backupStorage.StrFreeSize, result.StrFreeSize);


        }
    }
}
