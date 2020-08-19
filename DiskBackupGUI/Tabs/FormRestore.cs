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
using DiskBackupGUI.Tabs.RestoreTabs;
using NarDIWrapper;

namespace DiskBackupGUI.Tabs
{
    public partial class FormRestore : Form
    {
        Main myMain;
        public DiskTracker diskTracker;
        public int chooseDiskOrVolume = 2;
        private Form currentChildForm;
        public List<DataGridViewCell> viewCells;

        public class MyBackupMetadata
        {
            public char Letter { get; set; }
            public int BackupType { get; set; }
            public int Version { get; set; }
            public int OSVolume { get; set; }
            public char DiskType { get; set; }
        }

        public FormRestore(Main main)
        {
            myMain = main;
            InitializeComponent();
            diskTracker = new DiskTracker();
            //wipe disk-volume hazır hale geldiğinde alttaki kodu uncomment hale getir............                                      <-----------
            dgwRestore.DataSource = diskTracker.CW_GetBackupsInDirectory(myMain.myPath);

            ////dummy veriler
            //List<MyBackupMetadata> myBackups = new List<MyBackupMetadata>();
            //myBackups.Add(new MyBackupMetadata()
            //{
            //    BackupType = 1111,
            //    DiskType = 'a',
            //    Letter = 'z',
            //    OSVolume = 88888,
            //    Version = 999
            //});
            //myBackups.Add(new MyBackupMetadata()
            //{
            //    BackupType = 2222,
            //    DiskType = 'b',
            //    Letter = 'y',
            //    OSVolume = 77777,
            //    Version = 888
            //});
            //dgwRestore.DataSource = myBackups;

            dgwRestore.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        }

        private void OpenChildForm(Form childForm)
        {
            //open only form
            if (currentChildForm != null)
            {
                currentChildForm.Close();
            }
            currentChildForm = childForm;
            //End
            childForm.TopLevel = false;
            childForm.FormBorderStyle = FormBorderStyle.None;
            childForm.Dock = DockStyle.Fill;
            panelMain.Controls.Add(childForm);
            panelMain.Tag = childForm;
            childForm.BringToFront();
            childForm.Show();
        }

        private void dgwRestore_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            myMain.RtReportWrite(
                "Letter : " + dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["Letter"].Value.ToString()
                + "\nBackupType : " + dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["BackupType"].Value.ToString()
                + "\nVersion  : " + dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["Version"].Value.ToString()
                + "\nOSVolume : " + dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["OSVolume"].Value.ToString()
                + "\nDiskType : " + dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["DiskType"].Value.ToString()
                , false);
        }

        private void btnDisc_Click(object sender, EventArgs e)
        {
            MyBackupMetadata myBackup = new MyBackupMetadata();
            myBackup.Letter = (char)dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["Letter"].Value;
            myBackup.BackupType = Convert.ToInt32(dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["BackupType"].Value);
            myBackup.Version = Convert.ToInt32(dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["Version"].Value);
            myBackup.OSVolume = Convert.ToInt32(dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["OSVolume"].Value);
            myBackup.DiskType = (char)dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["DiskType"].Value;
            
            OpenChildForm(new RestoreDisk(myMain, myBackup));
        }

        private void btnVolume_Click(object sender, EventArgs e)
        {
            MyBackupMetadata myBackup = new MyBackupMetadata();
            myBackup.Letter = (char)dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["Letter"].Value;
            myBackup.BackupType = Convert.ToInt32(dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["BackupType"].Value);
            myBackup.Version = Convert.ToInt32(dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["Version"].Value);
            myBackup.OSVolume = Convert.ToInt32(dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["OSVolume"].Value);
            myBackup.DiskType = (char)dgwRestore.Rows[dgwRestore.CurrentRow.Index].Cells["DiskType"].Value;

            OpenChildForm(new RestoreVolume(myMain, myBackup));
        }
    }
}
