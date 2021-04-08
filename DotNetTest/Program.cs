using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using NarDIWrapper;

namespace DotNetTest
{
    class Program
    {

        static void Main(string[] args)
        {
            //NarDIWrapper.RestoreSource_Errors e;
            var rlist = DiskTracker.CW_GetBackupsInDirectory(args[0]);
            
            foreach(var item in rlist){
                Console.WriteLine(item.Letter);
                Console.WriteLine(item.Version);
                Console.WriteLine(item.VolumeTotalSize);
                Console.WriteLine(item.VolumeUsedSize);
                Console.WriteLine(item.SystemPartSize);
                Console.WriteLine(item.Fullpath);
                Console.WriteLine("##################");
            }


            BackupMetadata bm = new BackupMetadata();

            bm = rlist[0];
            {
                Console.WriteLine(bm.VolumeTotalSize);
                Console.WriteLine(bm.VolumeUsedSize);
                Console.WriteLine(bm.SystemPartSize);
                Console.WriteLine(bm.Fullpath);
            }
            
            RestoreStream stream = new RestoreStream(bm, args[0]);
            stream.SetDiskRestore(2, 'F', false, false, 'G');




            return;

        }

    }
}
