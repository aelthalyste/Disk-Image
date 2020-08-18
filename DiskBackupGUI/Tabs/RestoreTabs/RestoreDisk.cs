using NarDIWrapper;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace DiskBackupGUI.Tabs.RestoreTabs
{
    public partial class RestoreDisk : Form
    {
        Main myMain;
        FormRestore formRestore;
        FormRestore.MyBackupMetadata myBackupMetadata;
        public DiskTracker diskTracker;
        private int choseDisk = 1;
        private Form currentChildForm;

        public RestoreDisk(Main main, FormRestore.MyBackupMetadata backupMetadata)
        {
            //buraya formRestore'dan seçili olan myBackupMetadata geliyor
            InitializeComponent();
            formRestore = new FormRestore(myMain);
            myMain = main;
            myBackupMetadata = backupMetadata;
            diskTracker = new DiskTracker();
            dgwDisk.DataSource = diskTracker.CW_GetDisksOnSystem();
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

        private void RestoreDisk_Load(object sender, EventArgs e)
        {
            dgwDisk.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;

            myMain.RtReportWrite("Letter : " + myBackupMetadata.Letter.ToString()
                + "\nBackupType : " + myBackupMetadata.BackupType.ToString()
                + "\nVersion  : " + myBackupMetadata.Version.ToString()
                + "\nOSVolume : " + myBackupMetadata.OSVolume.ToString()
                + "\nDiskType : " + myBackupMetadata.DiskType.ToString(), false);
        }

        private void dgwDisk_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            myMain.RtReportWrite("", false);
        }

        private void btnRestore_Click(object sender, EventArgs e)
        {
            //aşşağıdaki koda ihtiyaç olmayabilir... 
            //**************************************************
            //**************************************************
            //              BURAYI TEKRAR DÜŞÜN
            //**************************************************
            //**************************************************

            if (txtDiskName.Text != "")
            {
                List<DataGridViewRow> checkedDgvDisk = new List<DataGridViewRow>();
                foreach (DataGridViewRow row in dgwDisk.Rows)
                {
                    if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
                    {
                        checkedDgvDisk.Add(row);
                    }
                }            }
            else
            {
                myMain.RtReportWrite("Textbox boş bırakılamaz", false);
            }
        }

        private void btnCancel_Click(object sender, EventArgs e)
        {
            OpenChildForm(new FormRestore(myMain));
        }
    }
}
