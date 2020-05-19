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

      unsafe
      {
        DiskTracker tracker = new DiskTracker();

        if (tracker.CW_InitTracker())
        {
          

          for (; ; )
          {
            string Input = Console.ReadLine();

            var parsed = Input.Split(',');
            if (parsed[0] == "add")
            {
              int type = 0;
              if (parsed[2] == "i")
              {
                type = 1;
              }


              if (!tracker.CW_AddToTrack(parsed[1][0], type))
              {
                Console.WriteLine("Can't start tracking volume\n");
              }

            }
            if (parsed[0] == "q")
            {
              break;
            }
            else if (parsed[0] == "f")
            {
              char Letter = parsed[1][0];

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
                      Console.Write("Cant read stream, operation failed\n");
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
            else if (parsed[0] == "d")
            {
              char Letter = parsed[1][0];

              StreamInfo info = new StreamInfo();
              if (!tracker.CW_SetupStream(Letter, info))
              {
                Console.WriteLine("Can't setup stream\n");
                continue;
              }
              FileStream st = File.Create(info.FileName);
              Console.WriteLine(info.ClusterCount);

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
                    Console.WriteLine("Cant read stream, operation failed\n");
                    succ = false;
                    break;
                  }

                }

                tracker.CW_TerminateBackup(succ);
                st.Close();
              }


            }
            else if (parsed[0] == "r")
            {
              char SrcLetter = parsed[1][0];
              char TargetLetter = parsed[2][0];

              int type = 0;
              if (parsed[4] == "i")
              {
                type = 1;
              }

              int version = Convert.ToInt32(parsed[3]);
              Console.Write("Version => ");
              Console.WriteLine(version);
              if (tracker.CW_RestoreVolumeOffline(TargetLetter, SrcLetter, 4096, version, type, ""))
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
          Console.WriteLine("Can't init tracker");
          // TODO err
        }

      }

    }

  }
}
