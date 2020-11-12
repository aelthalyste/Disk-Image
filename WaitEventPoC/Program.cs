using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WaitEventPoC
{
    class Program
    {
        static ManualResetEvent mrEvent = new ManualResetEvent(true);

        static void Main(string[] args)
        {
            //var backup = new BackupManager();
            //backup.CreateBackup();
            //Console.ReadLine();
            //backup.Pause();
            //Console.ReadLine();
            //backup.Resume();
            //Console.ReadLine();
            //backup.CancelTask();
            try
            {
                using (new NetworkConnection(@"\\10.34.0.146\source", new System.Net.NetworkCredential("cloudne", "123456")))
                {
                    var dirInfo = new DirectoryInfo(@"\\10.34.0.146\");
                    Console.WriteLine(dirInfo.Exists);
                }
            }
            catch (Exception e)
            {
                Console.WriteLine(e.Message);
            }
            Console.ReadLine();
        }
        static void Pause()
        {
            mrEvent.Reset();
        }

        static void Resume()
        {
            mrEvent.Set();
        }

        private static void CancellableFunction(CancellationToken token)
        {
            Task.Run(async () =>
            {
                for (int i = 0; i < 100; i++)
                {
                    if (token.IsCancellationRequested)
                    {
                        break;
                    }
                    Console.WriteLine(i);
                    await Task.Delay(100);
                }
            });
        }

        private static void WaitEventExample()
        {
            Task.Run(async () =>
            {
                for (int i = 0; i < 100; i++)
                {
                    mrEvent.WaitOne();
                    Console.WriteLine(i);
                    await Task.Delay(100);
                }
            });
            Console.ReadLine();
            mrEvent.Set();
            Thread.Sleep(1000);
            mrEvent.Reset();
            Thread.Sleep(2000);
            mrEvent.Set();
        }
    }
}
