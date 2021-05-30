using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using NarNative;

namespace DotNetTest
{
    class Program
    {

        static void Main(string[] args)
        {
            
            string extension = ".dll";
            
            List<string> resAllVolumes = NarNative.ExtensionSearcher.FindExtensionAllVolumes(extension);
            List<string> resSingleVolume = NarNative.ExtensionSearcher.FindExtension('C', extension);

            foreach (var f in resAllVolumes)
            {
                Console.WriteLine(f);
            }

            string b= Console.ReadLine();
            Console.WriteLine(b);
            return;

        }

    }
}
