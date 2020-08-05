using NarDIWrapper;
using Quartz;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace DiskBackupGUI
{
    public class BackupJob : IJob
    {
        public Task Execute(IJobExecutionContext context)
        {
            //liste varmış gibi kabul ediyoruz
            List<char> letters = Main.Instance.taskParams[context.JobDetail.Key.Name];
            var diskTracker = Main.Instance.diskTracker;
            int typeParam = context.PreviousFireTimeUtc == null? 0 : 1;

            int bufferSize = 64 * 1024 * 1024;
            byte[] buffer = new byte[bufferSize];
            StreamInfo str = new StreamInfo();
            long readSoFar = 0;
            bool result = false;
            foreach (var letter in letters)
            {
                if (diskTracker.CW_SetupStream(letter, typeParam, str))
                {
                    unsafe
                    {
                        fixed (byte* BAddr = &buffer[0])
                        {
                            FileStream file = File.Create(Main.Instance.path + str.FileName); //kontrol etme işlemine bak
                            while (diskTracker.CW_ReadStream(BAddr, bufferSize))
                            {
                                readSoFar += bufferSize;
                                file.Write(buffer, 0, bufferSize);

                            }
                            if (readSoFar != str.ClusterCount * str.ClusterSize)
                            {
                                long ReadRemaining = (long)(str.ClusterCount * str.ClusterSize - readSoFar);
                                if (ReadRemaining <= bufferSize)
                                {
                                    file.Write(buffer, 0, (int)ReadRemaining);
                                    result = true;
                                }
                            }
                            diskTracker.CW_TerminateBackup(result);
                        }
                    }
                }

            }
            return Task.CompletedTask;

        }

    }
}
