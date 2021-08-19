using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using NarNative;
using NarDIWrapper;
using System.Diagnostics;
namespace DotNetTest
{
    class Program
    {

        public int CompareTo(CSNarFileEntry other) {
            return 1;
        }

        static public void TEST_FULL() {
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

        static public void TEST_FULL_CLONE()
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

        static void Main(string[] args)
        {
            TEST_FULL();
            return;

            //var disklist = DiskTracker.CW_GetDisksOnSystem();
            //foreach (var disk in disklist)
            //{
            //    Console.WriteLine("{0} {1} {2}", disk.Type, disk.Size, disk.ID);
            //}
            //return;

            Console.WriteLine("Selected metadata is ", args[0]);

            CSNarFileExplorer FE = new CSNarFileExplorer(args[0]);
            while (true)
            {
                var list = FE.CW_GetFilesInCurrentDirectory();
                
                // list files in directory
                int i = 0;

                foreach (var item in list)
                {
                    Console.Write(i++);
                    Console.Write("\t\t");
                    Console.WriteLine("{0} {1} (mb : {2})",item.Name, item.Size, item.Size/(1024*1024));
                }
                
                // silly way to get input.
                string line = Console.ReadLine();
                int selection = Convert.ToInt32(line);

                
                if (selection < 0)
                {
                    FE.CW_PopDirectory();
                    continue;
                }

                
                CSNarFileEntry SelectedEntry = list[selection];
                if(SelectedEntry.IsDirectory){
                    FE.CW_SelectDirectory(SelectedEntry.UniqueID);
                }
                if (SelectedEntry.IsDirectory == false) {

                    // there are two ways to initiate restore. Both completely constructs identical outcomes.
                    // after initialization, one MUST check if stream initialized successfully by calling Stream.IsInit();
                    // if that call returns false, Stream is unusable and something went wrong.

                    CSNarFileExportStream Stream = null;
                    // First method: Create export stream directly from file explorer 
                    {
                        Stream = FE.CW_SetupFileRestore(SelectedEntry.UniqueID);
                    }

                    // Second method: Create export stream by passing file explorer object to it's constructor
                    {
                        Stream = new CSNarFileExportStream(FE, SelectedEntry.UniqueID);
                    }

                    // rest of the operation is simple. Call advance stream until it returns false. After  advance stream returns true,
                    // Caller is responsible for the managing output pipe. This example uses simple file I/O.
                    if (Stream.IsInit())
                    {
                        FileStream Output = File.Create(SelectedEntry.Name);
                        unsafe
                        {
                            ulong buffersize = 1024 * 1024 * 64;
                            byte[] buffer = new byte[buffersize];
                            fixed (byte* baddr = &buffer[0])
                            {
                                // After each AdvanceStream call, Stream object determines where caller must seek on it's output pipe,
                                // and how many bytes it must write. Caller is responsible for applying seek and write.

                                while (Stream.AdvanceStream(baddr, buffersize))
                                {
                                    // seek to desired offset and write to it
                                    Output.Seek((long)Stream.TargetWriteOffset, SeekOrigin.Begin);
                                    Output.Write(buffer, 0, (int)Stream.TargetWriteSize);
                                }

                                // Check if any error occured during stream. Termination may be due to some internal error.
                                if (Stream.Error != FileRestore_Errors.Error_NoError)
                                {
                                    Console.WriteLine("Unable to restore!");
                                    // warn user that something went wrong    
                                }
                                if (Stream.Error == FileRestore_Errors.Error_NoError)
                                {
                                    Console.WriteLine("Successfully restored!");
                                    // SUCCESS!
                                }

                            }

                        }
                        // Truncate file to it's original size.
                        Output.SetLength((long)Stream.TargetFileSize); // save to cast long(assuming long is a 64bit type)
                        Output.Close();

                    }
                    else {
                        Console.WriteLine("Unable to setup stream!\n");
                    }


                    Console.WriteLine("Done!\n");
                }
                

            }
            
            return;
        }

    }
}
