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
            DiskTracker dt = new DiskTracker();
            StreamInfo streamInfo = new StreamInfo();
            dt.CW_InitTracker();

            dt.CW_SetupStream('C', 0, streamInfo, true);
            int i = 0;
            while (true) {
                unsafe
                {
                    
                    int bfsize = 1024 * 1024 * 4;

                    fixed (byte* a = new Byte[bfsize])
                    { 
                        var r = dt.CW_ReadStream(a, 'C', bfsize);

                        if (r.Error != BackupStream_Errors.Error_NoError)
                        {
                            Console.WriteLine("Detected stream error");
                            break;
                        }
                        if (r.WriteSize == 0) 
                        {
                            Console.WriteLine("Write size returned 0");
                            break;
                        }

                        //Console.Write("\rStep : ", i++);

                    }


                }
            }
            dt.CW_TerminateBackup(false, 'C');

            return;

        }

    }
}
