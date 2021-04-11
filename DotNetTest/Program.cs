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
            if (dt.CW_InitTracker())
            {
                Console.WriteLine("Everything is ok\n");
            }
            else {
                Console.WriteLine("Unable to init tracker\n");
            }

            Console.Read();

            return;

        }

    }
}
