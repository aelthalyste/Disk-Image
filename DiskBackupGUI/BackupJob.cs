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
            long BytesReadSoFar = 0;
            int Read = 0;
            bool result = false;
            foreach (var letter in letters)
            {
                if (diskTracker.CW_SetupStream(letter, typeParam, str))
                {
                    unsafe
                    {
                        fixed (byte* BAddr = &buffer[0])
                        {
                            FileStream file = File.Create(Main.Instance.myPath + str.FileName); 
                            while (true)
                            {
                                Read = diskTracker.CW_ReadStream(BAddr, bufferSize);
                                if (Read == 0)
                                    break;
                                file.Write(buffer, 0, Read);
                                BytesReadSoFar += Read;
                            }
                            result = (long)str.ClusterCount * (long)str.ClusterSize == BytesReadSoFar;
                            diskTracker.CW_TerminateBackup(result);

                            try
                            {
                                File.Copy(str.MetadataFileName, Main.Instance.myPath + str.MetadataFileName);
                            }
                            catch (IOException iox)
                            {
                                MessageBox.Show(iox.Message);
                            }

                            if (result == true)
                            {
                                diskTracker.CW_SaveBootState();
                            }
                            file.Close();
                        }
                    }
                }
            }
            return Task.CompletedTask;
        }

    }
}
