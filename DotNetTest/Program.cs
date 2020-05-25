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

    static void PrintUsage()
    {
      Console.Write("##################\n");
      Console.Write("Kullanım şekli:\n" +
        "Bir volume'un backupunu almadan önce, \"ekle [Volume harfi] [diff veya inc yedek alma şekli]\" diyerek volumeu operasyona hazır hale getirmelisiniz\n" +
        "örnek kullanım : \"ekle C inc\" yaparsanız C volumeu incremental yedek alma prosedürü için hazır hale gelir. " +
        "\nBu noktadan sonra önce full sonra incremental alıp devam edilebilir\n" + 
        "Yedeklerin çıkartılacağı dizini değiştirmek için \"dizin [istenilen directory]\" yazabilirsiniz.\nYedeklenen dosyalar oraya aktarılacak, restore yapılırken de oraya bakılacaktır\n");

      Console.WriteLine("\n");

      PrintBackupUsage();
      Console.WriteLine();
      PrintRestoreUsage();
      Console.Write("##################\n");
    }

    static void PrintBackupUsage()
    {
      Console.WriteLine("------- BACKUP -------\n");
      Console.WriteLine("full [volume harfi]");
      Console.WriteLine("versiyon [volume harfi]");
      Console.WriteLine("Versiyon numaraları 1den başlar ve her istekte artar");
    }

    static void PrintRestoreUsage()
    {
      Console.WriteLine("----- RESTORE ------\n");
      Console.WriteLine("restore [kaynak] [hedef] [versiyon, full ise full sadece] [eğer full değil ise inc veya diff]");
      Console.WriteLine("\"restore [yedek alınmış olan volume harfi] [restore edilmek istenilen hedef volume] [restore versiyonu] [yedek alma tipi]\"" +
        "\nburadaki [restore versiyonu] bulunan kısma full yazılırsa fullbackupa geri dönecektir, onun dışında girilen bir tamsayı değeri belirtilen versiyona geri yükleme yapar\n" +
        "\nburada yedek alma tipine inc veya diff yazılmalıdır, eğer fullbackupa geri yükleme yapılacaksa, bu parametre boş bırakılabilir\n" +
        "Örn kullanım, C volume'u 5 incremental versiyona sahip olsun: \"restore C E full\" ya da \"restore C E 4 inc\"\n");
    }

    static void PrintCommands()
    {
      Console.WriteLine("------- COMMANDS --------\n" +
        "dizin [yeni dizin]\n" +
        "ekle [volume harfi] [inc veya diff prosedür adı]\n" +
        "full [volume harfi]\n" +
        "versiyon [volume harfi]\n" +
        "restore [kaynak] [hedef] [versiyon, full ise full sadece] [eğer full değil ise inc veya diff]\n");

    }

    static bool IsChar(char val) { return (val >= 'a' && val <= 'z') || (val >= 'A' && val <= 'Z'); }

    static void Main(string[] args)
    {

      unsafe
      {
        Console.WriteLine("Narbulut volume yedekleme servisi v0.1\n");
        PrintUsage();
        DiskTracker tracker = new DiskTracker();
        string RootDir = "";
        if (tracker.CW_InitTracker())
        {
          

          for (; ; )
          {
            var Input = Console.ReadLine().Split(' ');
            Input[0] = Input[0].ToLower();

            if(Input.Length <= 1)
            {
              if (Input[0] == "full" || Input[0] == "versiyon") PrintBackupUsage();
              else if (Input[0] == "restore" || Input[0] == "restore" ) PrintRestoreUsage();
              else if (Input[0] == "help" || Input[0] == "h") PrintUsage();
              else if (Input[0] == "komutlar") PrintCommands();
              else PrintUsage();
            }

            if(Input[0] == "ekle")
            {
              if (Input.Length != 3) PrintCommands();
              else if (Input[2] != "inc" && Input[2] != "diff") PrintCommands();
              else if(Input[1].Length != 1) PrintCommands();
              else
              {
                int type = 0;
                if (Input[2] == "inc") type = 1;
                else if (Input[2] == "diff") type = 0;
                else {
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
              if (!tracker.CW_SetupStream(Letter, streamInfo))
              {
                Console.WriteLine("Cant setup stream\n");
                continue;
              }

              FileStream st = File.Create(RootDir + streamInfo.FileName);

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
              st.Close();

            }

            else if (Input[0] == "versiyon")
            {

              char Letter = Input[1][0];
              StreamInfo info = new StreamInfo();
              if (!tracker.CW_SetupStream(Letter, info))
              {
                Console.WriteLine("Can't setup stream\n");
                continue;
              }
              FileStream st = File.Create(RootDir + info.FileName);

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

            else if (Input[0] == "restore")
            {
              if (Input.Length != 4 && Input.Length != 5 ) { PrintRestoreUsage(); continue; }
             else if (Input.Length == 4 && Input[3] != "full") { PrintRestoreUsage(); continue; }
             else if (Input.Length == 5 && Input[3] == "full") { PrintRestoreUsage(); continue; }
              else if (Input.Length == 5 && Input[4] != "diff" && Input[4] != "inc") { PrintRestoreUsage(); continue; }

              // restore target source versiyon|full inc|diff
              // 0        1       2       3            4
              char SrcLetter = Input[1][0];
              char TargetLetter = Input[2][0];

              int version = -1; // full restore
              if (Input.Length == 5) version = Convert.ToInt32(Input[3]) - 1;
              if (version <= 0) PrintRestoreUsage();

              int type = -1; //full restore
              if (Input.Length == 5)
              {
                if (Input[4] == "inc") type = 1;
                if (Input[4] == "diff") type = 0;
              }

              Console.Write("Version => ");
              Console.WriteLine(version);
              if (tracker.CW_RestoreVolumeOffline(TargetLetter, SrcLetter, 4096, version, type, RootDir))
              {
                Console.Write("Restored!\n");
              }
              else
              {
                Console.Write("Couldn't restore\n");
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
