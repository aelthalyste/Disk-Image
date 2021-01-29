using LicenseKeyGenerator.Core.DataAccess.EntityFramework;
using LicenseKeyGenerator.DataAccess.Abstract;
using LicenseKeyGenerator.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace LicenseKeyGenerator.DataAccess.Concrete.EntityFramework
{
    public class EfLicenseDal: EfEntityRepositoryBase<License, KeyGenaratorContext>, ILicenseDal
    {
    }
}
