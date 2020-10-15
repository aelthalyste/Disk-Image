﻿using DiskBackup.Entities.Concrete;
using DiskBackup.Core.DataAccess.EntityFramework;
using DiskBackup.DataAccess.Abstract;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Concrete.EntityFramework
{
    public class EfActivityLogDal : EfEntityRepositoryBase<ActivityLog, ImageDiskContext>, IActivityLogDal
    {
    }
}
