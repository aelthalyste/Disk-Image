﻿using System;
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

    static void PrintCommands()
    {
      Console.WriteLine("------- COMMANDS --------\n" +
        "dizin [dosyaların çıkartılacağı ve okunacağı dizin]\n" +
        "ekle [volume harfi] [inc veya diff prosedür adı]\n" +
        "full [volume harfi]\n" +
        "versiyon [volume harfi]\n" +
        "restore [kaynak] [hedef] [versiyon] (hedef volumeunun var olması gerekir)\n" +
        "restore [kaynak] [hedef] [versiyon] [disk id] (gerekli hedef volumeu, belirtilen disk numarasında otomatik oluşturur)\n"
        );
    }

    static void PrintExampleUsage()
    {
      Console.WriteLine("--- ORNEK KULLANIM ----\n" +
        "ekle C inc\n" +
        "ekle E diff\n" +
        "full E\n" +
        "versiyon E" +
        "restore E D 1 [yedeklenen E volumeunun 1. versiyonunu, D volumuna geri yükler]\n" +
        "restore C M 0 [yedeklenen C volumeunun 0. versiyonunu, M volumeuna geri yükler]\n" +
        "restore C D 2 1 [yedeklenen C volumeunun 2. versiyonunu, birinci diski tamamen silip içerisinde sıfırdan oluşturur]\n"
        );
    }

    static bool IsChar(char val) { return (val >= 'a' && val <= 'z') || (val >= 'A' && val <= 'Z'); }

    static void Main(string[] args)
    {
      int BufferSize = 1024 * 1024 * 64;
      unsafe
      {
        Console.WriteLine("Narbulut volume yedekleme servisi v0.1\n");
        PrintCommands();
        DiskTracker tracker = new DiskTracker();
        string RootDir = "";

        var test = tracker.CW_GetVolumes();

        if (tracker.CW_InitTracker())
        {
          
          for (; ; )
          {
            var RawInput = Console.ReadLine();
            var Input = RawInput.Split(' ');
            Input[0] = Input[0].ToLower();
            if (RawInput == "\n") continue;
            if (RawInput == "örnek" || RawInput == "ornek")
            {
              PrintExampleUsage();
              continue;
            }
            if (RawInput.ToLower() == "commands" || RawInput.ToLower() == "cmd")
            {
              PrintCommands();
              continue;
            }
            if (Input.Length <= 1)
            {
              if (Input[0] == "full" || Input[0] == "versiyon") PrintCommands();
              else if (Input[0] == "restore" || Input[0] == "restore") PrintCommands();
              else if (Input[0] == "help" || Input[0] == "h") PrintCommands();
              else if (Input[0] == "komutlar") PrintCommands();
              else PrintCommands();
              continue;
            }


            if (Input[0] == "ekle")
            {
              if (Input.Length != 3) PrintCommands();
              else if (Input[2] != "inc" && Input[2] != "diff") PrintCommands();
              else if (Input[1].Length != 1) PrintCommands();
              else
              {
                int type = 0;
                if (Input[2] == "inc") type = 1;
                else if (Input[2] == "diff") type = 0;
                else
                {
                  PrintCommands();
                  continue;
                }


                
                if (!tracker.CW_AddToTrack(Input[1][0], type)) Console.WriteLine("Can't start tracking volume\n");

              }
            }

            if (Input[0] == "full")
            {
              
              char Letter = Input[1][0];
              StreamInfo streamInfo = new StreamInfo();
              if (!tracker.CW_SetupStream(Letter, 1, streamInfo))
              {
                Console.WriteLine("Cant setup stream\n");
                continue;
              }
              Console.WriteLine(RootDir + streamInfo.FileName);
              
              FileStream st = File.Create(RootDir + streamInfo.FileName);

              byte[] Buffer = new byte[BufferSize];
              fixed (byte* BAddr = &Buffer[0]) {
                while (tracker.CW_ReadStream(BAddr, BufferSize))
                {
                  st.Write(Buffer, 0, BufferSize);
                }
              }
              if (!tracker.CW_TerminateBackup(true)) {
                Console.WriteLine("Can't terminate backup\n");
              }

              st.Close();


            }

            else if (Input[0] == "versiyon")
            {

              char Letter = Input[1][0];
              StreamInfo info = new StreamInfo();
              if (!tracker.CW_SetupStream(Letter, 1, info))
              {
                Console.WriteLine("Can't setup stream\n");
                continue;
              }
              FileStream st = File.Create(RootDir + info.FileName);

              byte[] buffer = new byte[BufferSize];
              fixed (byte* BAdd = &buffer[0])
              {
                while(tracker.CW_ReadStream(BAdd, BufferSize)){
                  st.Write(buffer, 0, BufferSize);
                }
                tracker.CW_TerminateBackup(true);
                st.Close();
              }


            }

            else if (Input[0] == "restore")
            {
              if (Input.Length == 4 || Input.Length == 5)
              {
                Console.Write("Arg count ");
                Console.WriteLine(Input.Length);
                // restore target source version
                // 0        1       2       3  
                char SrcLetter = Input[1][0];
                char TargetLetter = Input[2][0];
                int version;
                if (Input[3].ToLower() == "full")
                {
                  version = -1;
                }
                else
                {
                  version = System.Convert.ToInt32(Input[3]);
                }

                if (Input.Length == 5)
                {
                  Console.WriteLine("Fresh disk restore starting\n");
                  int DiskID = System.Convert.ToInt32(Input[4]);
                  Console.Write("DiskID => ");
                  Console.WriteLine(DiskID);
                  if (!tracker.CW_RestoreToFreshDisk(TargetLetter, SrcLetter, version, DiskID, RootDir))
                  {
                    Console.WriteLine("Couldn't restore\n");
                  }
                  continue;
                }

                if (tracker.CW_RestoreToVolume(TargetLetter, SrcLetter, version, true, RootDir))
                {
                  Console.Write("Restored!\n");
                }
                else
                {
                  Console.Write("Couldn't restore\n");
                }
              }

            }

            else if (Input[0] == "q") break;
          }

        }
        else
        {
          Console.WriteLine("Can't init tracker");
          // TODO err
        }

      }

    }

  }
}
