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


        static void Main(string[] args)
        {

            CSNarFileExplorer FE = new CSNarFileExplorer("G:\\NB_M_2-C07291017.nbfsm");
            while (true)
            {
                var list = FE.CW_GetFilesInCurrentDirectory();
                
                // list files in directory
                int i = 0;
                foreach (var item in list)
                {
                    Console.Write(i++);
                    Console.Write("\t\t");
                    Console.WriteLine(item.Name);
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
                    FE.CW_SelectDirectory(SelectedEntry);
                }
                if (SelectedEntry.IsDirectory == false) {

                    // there are two ways to initiate restore. Both completely constructs identical outcomes.
                    // after initialization, one MUST check if stream initialized successfully by calling Stream.IsInit();
                    // if that call returns false, Stream is unusable and something went wrong.

                    CSNarFileExportStream Stream = null;
                    // First method: Create export stream directly from file explorer 
                    {
                        Stream = FE.CW_SetupFileRestore(SelectedEntry);
                    }

                    // Second method: Create export stream by passing file explorer object to it's constructor
                    {
                        Stream = new CSNarFileExportStream(FE, SelectedEntry);
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
                                if (Stream.Error != FileRestore_Errors.Error_NoError) { 
                                    // warn user that something went wrong    
                                }
                                if (Stream.Error == FileRestore_Errors.Error_NoError) { 
                                    // SUCCESS!
                                }

                            }

                        }
                        // Truncate file to it's original size.
                        Output.SetLength((long)Stream.TargetFileSize); // save to cast long(assuming long is a 64bit type)
                        Output.Close();

                    }


                    Console.WriteLine("Done!\n");
                }
                

            }
            
            return;
        }

    }
}
