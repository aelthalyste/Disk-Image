using NarDIWrapper;
using Quartz;
using Quartz.Impl;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
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
        public IScheduler Scheduler { get; set; }

        public MyMessageBox()
        {
            InitializeComponent();
        }

        private void btnOkay_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void btnRepeatOkay_Click(object sender, EventArgs e)
        {
            try
            {
                var repeat = Convert.ToInt32(txtRepeatTime.Text);
                var repeatCount = Convert.ToInt32(txtRepeatCount.Text);
                var startDate = repeatDatePicker.Value;
                var startTime = repeatTimePicker.Value + (startDate - DateTime.Now);
                var trigger = TriggerBuilder.Create()
               .WithIdentity("trigger3", "group1")
               .StartAt(DateTimeOffset.Now.Add(startTime - DateTime.Now)) // if a start time is not given (if this line were omitted), "now" is implied
               .WithSimpleSchedule(x => x
                   .WithIntervalInMinutes(repeat)
                   .WithRepeatCount(repeatCount))
               .Build();
                Close();

            }
            catch (Exception)
            {
                MessageBox.Show("Tekrarlanıcak Süreyi Dakika Cinsinden Yazınız!!!");
            }
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
