using DiskBackup.DataAccess.Entities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Entities.Concrete
{
    public class EmailInfo: IEntity
    {
        public int Id { get; set; }
        public string EmailAddress { get; set; }
    }
}
