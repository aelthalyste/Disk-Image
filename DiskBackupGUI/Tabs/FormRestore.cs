using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using NarDIWrapper;

namespace DiskBackupGUI.Tabs
{
    public partial class FormRestore : Form
    {
        Main myMain;

        public FormRestore(Main main)
        {
            myMain = main;
            InitializeComponent();
        }

        private void FormRestore_Load(object sender, EventArgs e)
        {
            FirstShow();
            dgwVolume.DataSource = myMain.volumes;
            dgwVolume.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        }

        private void btnDisc_Click(object sender, EventArgs e)
        {
            DiskShow();
            myMain.RtReportWrite("Disk seçildi", false);
        }

        private void btnVolume_Click(object sender, EventArgs e)
        {
            myMain.RtReportWrite("Volume seçildi", false);
            VolumeShow();
        }
        public void FirstShow()
        {
            btnVolume.Visible = true;
            btnDisc.Visible = true;
            dgwRestore.Visible = true;
            btnDisc.BringToFront();
            btnVolume.BringToFront();
            dgwVolume.Visible = false;
            dgwDisk.Visible = false;
            btnCancel.Visible = false;
            btnRestore.Visible = false;
        }
        public void VolumeShow()
        {
            btnVolume.Visible = false;
            dgwRestore.Visible = false;
            btnDisc.Visible = false;
            dgwVolume.Visible = true;
            btnCancel.Visible = true;
            btnRestore.Visible = true;
        }
        public void DiskShow()
        {
            btnVolume.Visible = false;
            btnDisc.Visible = false;
            dgwRestore.Visible = false;
            dgwVolume.Visible = false;
            dgwDisk.Visible = true;
            btnCancel.Visible = true;
            btnRestore.Visible = true;
        }

        static async Task<string> MyMessage(string denemeMessage)
        {
            await Task.Delay(5000);
            MessageBox.Show(denemeMessage);
            return null;
        }

        private async void btnRestore_Click(object sender, EventArgs e)
        {
            Task<string> task = MyMessage("asdasdasdasdasd");
            await Task.Delay(100);
            FirstShow();
            myMain.RtReportWrite("\nRestore İşlemi Başlatıldı", true);
        }

        private void btnCancel_Click(object sender, EventArgs e)
        {
            FirstShow();
            myMain.RtReportWrite("İşlem İptal Edildi", false);
        }

        private void dgwVolume_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            var size = dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["Size"].Value;
            var letter = dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["Letter"].Value;
            var diskId = dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["DiskId"].Value;
            var diskType = dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["DiskType"].Value;
            var bootable = dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["Bootable"].Value;
            myMain.RtReportWrite("Size : " + size.ToString() + "\nLetter : " + letter.ToString() + "\nDiskId : " + diskId.ToString() + "\nDiskType : " + diskType.ToString() + "\nBootable : " + bootable.ToString(), false);
           
        }
    }
}
