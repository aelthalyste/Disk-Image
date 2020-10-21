﻿using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class BackupStorageInfo : IEntity
    {
        //ListView'e doldururken üzerinden tekrar düşün sorun olursa
        //Yedekleme alanları & ekle 
        public int Id { get; set; }
        public string StorageName { get; set; }
        public BackupStorageType Type { get; set; } 
        public string Description { get; set; }
        public string StrCapacity { get; set; }
        public long Capacity { get; set; }
        public string StrUsedSize { get; set; }
        public long UsedSize { get; set; }
        public string StrFreeSize { get; set; }
        public long FreeSize { get; set; }
        public string Path { get; set; }
        public bool IsCloud { get; set; }   //true olduğunda aslında modelimiz hybrit oluyor
        public string Domain { get; set; } 
        public string Username { get; set; }
        public string Password { get; set; }
    }

    public enum BackupStorageType
    {
        Windows = 0,
        NAS = 1,
        Hybrit = 2
    }
    
}