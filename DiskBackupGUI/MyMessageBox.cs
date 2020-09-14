using Microsoft.Identity.Client;
using NarDIWrapper;
using Quartz;
using Quartz.Impl;
using Quartz.Impl.Matchers;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using static Quartz.Logging.OperationName;

namespace DiskBackupGUI
{
    public partial class MyMessageBox : Form
    {
        public IScheduler MyScheduler { get; set; }
        public static int jobIdCounter = 0;
        public List<char> Letters { get; set; }
        StreamWriter writer;
        private int IncOrDiff;

        public MyMessageBox(int DiffOrInc)
        {
            IncOrDiff = DiffOrInc;
            InitializeComponent();
        }

        //Görev oluşturma
        private async void btnOkay_Click(object sender, EventArgs e)
        {
            var startDate = datePicker.Value;
            var startTime = timepicker.Value + (startDate - DateTime.Now);
            try
            {
                btnOkay.Enabled = false;

                IJobDetail job = JobBuilder.Create<BackupJob>()
                               .WithIdentity("job" + jobIdCounter, "group")
                               .Build();
                var trigger = TriggerBuilder.Create()
                   .WithIdentity("trigger" + jobIdCounter, "group")
                   .StartAt(DateTimeOffset.Now.Add(startTime - DateTime.Now)) // if a start time is not given (if this line were omitted), "now" is implied
                   .WithSimpleSchedule(x => x
                       .WithIntervalInMinutes(1)
                       .WithRepeatCount(0))
                   .Build();
                await MyScheduler.ScheduleJob(job, trigger);
                Main.Instance.taskParams["job" + jobIdCounter] = Letters;
                jobIdCounter++;
                Close();
                ISchedulerFactory sf = new StdSchedulerFactory();
            }
            catch (Exception)
            {
                MessageBox.Show("Beklenmeyen bir durum oluştu");
            }
            finally
            {
                btnOkay.Enabled = true;
            }

            var allTriggerKeys = MyScheduler.GetTriggerKeys(GroupMatcher<TriggerKey>.AnyGroup());
            foreach (var triggerKey in allTriggerKeys.Result)
            {
                var triggerdetails = MyScheduler.GetTrigger(triggerKey);
                var Jobdetails = MyScheduler.GetJobDetail(triggerdetails.Result.JobKey);
                Console.WriteLine("IsCompleted -" + triggerdetails.IsCompleted + " |  TriggerKey  - " + triggerdetails.Result.Key.Name + " Job key -" + triggerdetails.Result.JobKey.Name + "| Deneme |" + Jobdetails.Result.Key.Name);
            }

            string fileName = @"SystemLog.txt";
            writer = new StreamWriter(fileName, true);
            var time = DateTime.Now;
            if (IncOrDiff == 1)
            {
                writer.WriteLine("Incremental Görev Oluşturuldu " + time.ToString() + "\n      -->Görev Başlangıç Tarihi : " + startDate + "\n      -->Görev Başlangıç Süresi : " + startTime);
            }
            else if (IncOrDiff == 0)
            {
               
                writer.WriteLine("Differential Görev Oluşturuldu " + time.ToString() + "\n      -->Görev Başlangıç Tarihi : " + startDate + "\n      -->Görev Başlangıç Süresi : " + startTime);
               
            }
            writer.Close();
        }

        //Tekrarlanan Görev Oluşturma
        private async void btnRepeatOkay_Click(object sender, EventArgs e)
        {
            var repeat = Convert.ToInt32(txtRepeatTime.Text);
            var repeatCount = Convert.ToInt32(txtRepeatCount.Text);
            try
            {
                btnRepeatOkay.Enabled = false;

                var startDate = repeatDatePicker.Value;
                var startTime = repeatTimePicker.Value + (startDate - DateTime.Now);
                IJobDetail job = JobBuilder.Create<BackupJob>()
                               .WithIdentity("job2" + jobIdCounter, "group2")
                               .Build();
                var trigger = TriggerBuilder.Create()
                   .WithIdentity("trigger2" + jobIdCounter, "group2")
                   .StartAt(DateTimeOffset.Now.Add(startTime - DateTime.Now)) // if a start time is not given (if this line were omitted), "now" is implied
                   .WithSimpleSchedule(x => x
                       .WithIntervalInMinutes(repeat)
                       .WithRepeatCount(repeatCount))
                   .Build();
                await MyScheduler.ScheduleJob(job, trigger);
                Main.Instance.taskParams["job2" + jobIdCounter] = Letters;
                jobIdCounter++;
                Close();

            }
            catch (Exception ex)
            {
                MessageBox.Show("Girilen inputlar hatalı biçimdeydi");
            }
            finally
            {
                btnRepeatOkay.Enabled = true;
            }

            string fileName = @"SystemLog.txt";
            writer = new StreamWriter(fileName, true);
            var time = DateTime.Now;

            if (IncOrDiff == 1)
            {
                writer.WriteLine("Tekrarlanan Incremental Backup Görevi Oluşturuldu " + time.ToString() + "\n      -->Tekrar Sayısı : " + repeatCount + "\n      -->Tekrar Süresi : " + repeat);
                
            }
            else if (IncOrDiff == 0)
            {
                writer.WriteLine("Tekrarlanan Differential Backup Görevi Oluşturuldu " + time.ToString() + "\n      -->Tekrar Sayısı : " + repeatCount + "\n      -->Tekrar Süresi : " + repeat);
            }
            writer.Close();
        }

        //Şimdi çalıştır 
        private async void btnNowOkay_Click(object sender, EventArgs e)
        {
            btnNowOkay.Enabled = false;
            IJobDetail job = JobBuilder.Create<BackupJob>()
                           .WithIdentity("job3" + jobIdCounter, "group3")
                           .Build();
            var trigger = TriggerBuilder.Create()
               .WithIdentity("trigger3" + jobIdCounter, "group3")
               .WithSimpleSchedule(x => x
                   .WithIntervalInMinutes(1)
                   .WithRepeatCount(0))
               .Build();
            await MyScheduler.ScheduleJob(job, trigger);
            Main.Instance.taskParams["job3" + jobIdCounter] = Letters;
            jobIdCounter++;
            Close();

            string fileName = @"SystemLog.txt";
            writer = new StreamWriter(fileName, true);
            var time = DateTime.Now;
            btnNowOkay.Enabled = true;
            if (IncOrDiff == 1)
            {
                writer.WriteLine("Şimdi Çalıştırılan Incremental Backup Oluşturuldu " + time.ToString());
            }
            else if (IncOrDiff == 0)
            {
                writer.WriteLine("Şimdi Çalıştırılan Differential Backup Oluşturuldu " + time.ToString());
            }
            writer.Close();
        }

        //form kapatma
        private void btnExit_Click(object sender, EventArgs e)
        {
            Close();
        }

        #region Form Hareket 

        //Formu hareket ettirmek ve panel üzerinden kontrol etmek için

        [DllImport("user32.DLL", EntryPoint = "ReleaseCapture")]
        private extern static void ReleaseCapture();

        [DllImport("user32.DLL", EntryPoint = "SendMessage")]
        private extern static void SendMessage(System.IntPtr hWnd, int wMsg, int wParam, int lParam);

        //formun kendisiyle hareket ettirmek için
        private void MyMessageBox_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }

        //formun üzerindeki panelden hareket ettirmek için
        private void panelTop_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }
        //formun sağ kısmındaki panelden hareket ettirmek için
        private void panel1_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }

        //formun sol kısmındaki panelden hareket ettirmek için
        private void panel2_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }
        #endregion
    }
}
