using LicenseKeyGenerator.Core.DataAccess;
using LicenseKeyGenerator.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace LicenseKeyGenerator.DataAccess.Abstract
{
    public interface ILicenseDal : IEntityRepository<License>
    {
    }
}
