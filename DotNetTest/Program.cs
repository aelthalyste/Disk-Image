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

            RestoreStream a = new RestoreStream(new BackupMetadata(), "adsf");

            if (a.SrcError == RestoreSource_Errors.Error_Decompression) { 
            
            }
            
            var b = RestoreSource_Errors.Error_InsufficentBufferSize;

        }

    }
}
