using LicenseKeyGenerator.Core.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace LicenseKeyGenerator.Entities
{
    public class License: IEntity
    {
        public string UniqKey { get; set; }
        public string DealerName { get; set; }
        public string CustomerName { get; set; }
        public string AuthorizedPerson { get; set; }
        public MachineType MachineType { get; set; }
        public DateTime SupportEndDate { get; set; }
        public DateTime CreatedDate { get; set; }
        public VersionType LicenseVersion { get; set; }
        public string Key { get; set; }
    }

    public enum MachineType
    {
        PhysicalMachine = 0,
        VirtualMachine = 1
    }

    public enum VersionType
    {
        Server = 0,
        Workstation = 1,
        SBS = 2
    }
}
