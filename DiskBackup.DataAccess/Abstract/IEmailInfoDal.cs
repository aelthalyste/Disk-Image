﻿using DiskBackup.DataAccess.Core;
using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.DataAccess.Abstract
{
    public interface IEmailInfoDal : IEntityRepository<EmailInfo>
    {
    }
}