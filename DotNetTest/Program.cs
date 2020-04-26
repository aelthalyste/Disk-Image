using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using NarDIWrapper;
using System.IO;

namespace DotNetTest
{
  class Program
  {
    static void Main(string[] args)
    {
      char Letter = 'E';
      unsafe
      {
        DiskTracker tracker = new DiskTracker();

        if (tracker.CW_InitTracker())
        {
          if (tracker.CW_AddToTrack(Letter, 0))
          {
            for (; ; )
            {
              string Input = Console.ReadLine();
              if (Input == "q")
              {
                break;
              }
              else if (Input == "f")
              {
                StreamInfo streamInfo = new StreamInfo();
                if (tracker.CW_SetupStream(Letter, streamInfo))
                {
                  
                  FileStream st = File.Create(streamInfo.FileName);
                  Console.WriteLine(streamInfo.FileName);
                  Console.WriteLine(streamInfo.MetadataFileName);

                  Console.Write("CLUSTER SIZE-> ");
                  Console.WriteLine(streamInfo.ClusterSize);
                  Console.Write("ClusterCount-> ");
                  Console.WriteLine(streamInfo.ClusterCount);

                  for (int i = 0; i < streamInfo.ClusterCount; i++)
                  {
                    byte[] Buffer = new byte[4096];
                    fixed (byte* BAddr = &Buffer[0])
                    {
                      if (tracker.CW_ReadStream(BAddr, 4096))
                      {
                        st.Write(Buffer, 0, 4096);
                      }
                      else
                      {
                        Console.Write("Cant read stream ");
                        Console.Write(i);
                        Console.Write("\t");
                        Console.WriteLine(streamInfo.ClusterCount);
                        break;
                      }
                    }
                  }
                  if (!tracker.CW_TerminateBackup(true))
                  {
                    Console.WriteLine("Can't terminate backup\n");
                  }
                  Console.WriteLine("C# FF\n");
                  st.Close();

                }


              }
              else if (Input == "d")
              {

                StreamInfo info = new StreamInfo();
                if (!tracker.CW_SetupStream(Letter, info))
                {
                  Console.WriteLine("Can't setup stream\n");
                  continue;
                }
                FileStream st = File.Create(info.FileName);
                Console.WriteLine(info.FileName);
                Console.WriteLine(info.MetadataFileName);

                byte[] buffer = new byte[4096];
                fixed (byte* BAdd = &buffer[0])
                {
                  bool succ = true;
                  for (int i = 0; i < info.ClusterCount; i++)
                  {
                    if (tracker.CW_ReadStream(BAdd, 4096))
                    {
                      st.Write(buffer, 0, 4096);
                    }
                    else
                    {
                      Console.WriteLine("Cant read stream\n");
                      succ = false;
                      break;
                    }

                  }
                  
                  tracker.CW_TerminateBackup(succ);
                  st.Close();
                }
                

              }
              else if (Input == "r")
              {

                if (tracker.CW_RestoreVolumeOffline('E', Letter, 4096, 1, 0))
                {
                  Console.Write("Restored!\n");
                }
                else
                {
                  Console.Write("Couldn't restore\n");
                }

              }
            }
          }
          else
          {
            Console.WriteLine("Can't add to track\n");
            // TODO err
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
