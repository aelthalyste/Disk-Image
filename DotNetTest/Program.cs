using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using NarNative;
using System.Diagnostics;
namespace DotNetTest
{
    class Program
    {

        static void Main(string[] args)
        {
            
            var start = Stopwatch.StartNew();
            Console.ReadLine();
            var result = ExtensionSearcher.FindExtension('C', ".dll");

            Console.WriteLine(start.ElapsedMilliseconds);

//            foreach (var item in result)
//            {
//                Console.WriteLine(item);
//            }
            Console.ReadLine();
            return;
        }

    }
}
