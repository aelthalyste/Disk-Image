using NarDIWrapper;
using System;
using System.IO;

namespace FullBackupPoc
{
    class Program
    {
        static void Main(string[] args)
        {
            TEST_FULL();
        }
        public static void TEST_FULL()
        {
            Console.WriteLine("Waiting input(letter): ");
            var inp = Console.ReadLine();
            StreamInfo Inf = new StreamInfo();

            var ID = DiskTracker.CW_SetupFullOnlyStream(Inf, inp[0], true);
            var Output = File.Create(Inf.FileName);
            unsafe
            {
                uint buffersize = 1024 * 1024 * 64;
                byte[] buffer = new byte[buffersize];
                fixed (byte* baddr = &buffer[0])
                {
                    while (true)
                    {
                        var ReadResult = DiskTracker.CW_ReadFullOnlyStream(ID, baddr, buffersize);
                        if (ReadResult.Error != BackupStream_Errors.Error_NoError)
                        {
                            break;
                        }
                        if (ReadResult.WriteSize == 0)
                        {
                            break;
                        }
                        Output.Write(buffer, 0, (int)ReadResult.WriteSize);
                    }
                }
            }

            // backup bir dosyaya yaziliyorsa, daha sonradan restore edilecekse metadatanin kayit edilmesi gerekiyor. true verilmeli arguman
            DiskTracker.CW_TerminateFullOnlyBackup(ID, true);
            Console.WriteLine("done!");
        }

        public static void TEST_FULL_CLONE()
        {
            Console.WriteLine("Waiting input(letter): ");
            var inp = Console.ReadLine();
            StreamInfo Inf = new StreamInfo();

            var ID = DiskTracker.CW_SetupDiskCloneStream(Inf, inp[0]);

            // Volume a yazmak icin acmaniz lazim, nasil olur bilmiyorum .NET de, gerekli kodu file.create ile degistirirsiniz.
            var Output = File.Create(Inf.FileName);
            unsafe
            {
                uint buffersize = 1024 * 1024 * 64;
                byte[] buffer = new byte[buffersize];
                fixed (byte* baddr = &buffer[0])
                {
                    while (true)
                    {
                        var ReadResult = DiskTracker.CW_ReadFullOnlyStream(ID, baddr, buffersize);
                        if (ReadResult.Error != BackupStream_Errors.Error_NoError)
                        {
                            break;
                        }
                        if (ReadResult.WriteSize == 0)
                        {
                            break;
                        }

                        Output.Seek((long)ReadResult.ReadOffset, SeekOrigin.Begin);
                        Output.Write(buffer, 0, (int)ReadResult.WriteSize);
                    }
                }
            }

            // disk klonlaniyorsa metadatanin kayit edilmesine gerek yok, false verilerek hizlica tamamlanabilir islem.
            DiskTracker.CW_TerminateFullOnlyBackup(ID, false);
            Console.WriteLine("done!");
        }

    }
}
