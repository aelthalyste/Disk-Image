﻿using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class FilesInBackup : IEntity
    {
        //File Explorer penceresinde bulunan listview'a gelecek model
        public ulong Id { get; set; }
        public FileType Type { get; set; }
        public string Name { get; set; }
        public string UpdatedDate { get; set; }
        public string StrSize { get; set; }
        public long Size { get; set; }
        public string Path { get; set; }
    }

    public enum FileType 
    { 
        Folder = 1,
        File = 0
    }
}
