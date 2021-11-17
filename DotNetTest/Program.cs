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

    class CsDirectoryExporter {
        
        public string SubDirectory;
        public string FileName;
        public ulong  FileID;

        public FileStream PrepareStream(string TargetPath, CSNarFileExplorer explorer, out CSNarFileExportStream StreamOut) {
            Directory.CreateDirectory(TargetPath + SubDirectory);
            StreamOut = new CSNarFileExportStream(explorer, FileID);
            return File.Create(TargetPath + SubDirectory + FileName);
        }

        public static List<CsDirectoryExporter> TraverseDirectory(CSNarFileExplorer FE, ulong UniqueDirectoryID)
        {
            List<CsDirectoryExporter> Result = new List<CsDirectoryExporter>();
            Result.Capacity = 1024 * 1024 * 4;

            FE.CW_SelectDirectory(UniqueDirectoryID);
            string BaseDirectory = FE.CW_GetCurrentDirectoryString();

            Stack<ulong> DirectoriesToVisit = new Stack<ulong>();
            DirectoriesToVisit.Push(UniqueDirectoryID);

            while (DirectoriesToVisit.Count > 0)
            {
                ulong CurrentDirUniqueID = DirectoriesToVisit.Pop();
                if (FE.CW_SelectDirectory(CurrentDirUniqueID))
                {
                
                    foreach (var file in FE.CW_GetFilesInCurrentDirectory())
                    {
                        if (file.IsDirectory)
                        {
                            DirectoriesToVisit.Push(file.UniqueID);
                        }
                        else
                        {
                            var element = new CsDirectoryExporter();

                            element.SubDirectory = FE.CW_GetCurrentDirectoryString().Remove(0, BaseDirectory.Length);
                            element.SubDirectory += @"\";
                            element.FileName = file.Name;
                            element.FileID = file.UniqueID;
                            Result.Add(element);
                        }
                    }
                    FE.CW_PopDirectory();
                
                }
            }

            return Result;
        }

    };



    class Program
    {



        static void Main(string[] args)
        {



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

                    // var FilesToRestore = CsDirectoryExporter.TraverseDirectory(FE, SelectedEntry.UniqueID);
                    // foreach (var item in FilesToRestore)
                    // {
                    // 
                    //     CSNarFileExportStream CSStream;
                    //     var OutputFile = item.PrepareStream("D:\\my_output_directory\\", FE, out CSStream);
                    // 
                    //     // usual stream operations
                    //     // while (stream.AdvanceStream(...)) { 
                    //     //    seek and write
                    //     // }
                    // 
                    //     // cleanup as usual
                    //     OutputFile.Close();
                    //     CSStream.FreeStreamResources();
                    // 
                    // }

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
