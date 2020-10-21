﻿using DiskBackup.Entities.Concrete;
using DiskBackup.DataAccess.Core;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Abstract
{
    public interface ITaskInfoDal:IEntityRepository<TaskInfo>
    {
    }
}