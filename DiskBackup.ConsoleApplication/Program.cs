using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.ConsoleApplication
{
    public class Program
    {
        static void Main(string[] args)
        {
            Data data = new Data();
            data.Start();
            Console.ReadLine();
        }
    }
}
