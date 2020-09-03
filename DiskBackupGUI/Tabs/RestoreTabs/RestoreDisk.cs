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
        List<MyDiskInfo> myDiskInfo;

        public RestoreDisk(Main main, FormRestore.MyBackupMetadata backupMetadata)
        {
            //buraya formRestore'dan seçili olan myBackupMetadata geliyor
            InitializeComponent();
            formRestore = new FormRestore(myMain);
            myMain = main;
            myBackupMetadata = backupMetadata;
            diskTracker = new DiskTracker();
            myDiskInfo = new List<MyDiskInfo>();

            foreach (var item in diskTracker.CW_GetDisksOnSystem())
            {
                if (item.Type == 'R')
                {
                    myDiskInfo.Add(new MyDiskInfo(){
                        Size = item.Size,
                        ID = item.ID,
                        MyType = "RAW"
                    });
                }
                else if (item.Type == 'M')
                {
                    myDiskInfo.Add(new MyDiskInfo()
                    {
                        Size = item.Size,
                        ID = item.ID,
                        MyType = "MBR"
                    });
                }
                else if (item.Type == 'G')
                {
                    myDiskInfo.Add(new MyDiskInfo()
                    {
                        Size = item.Size,
                        ID = item.ID,
                        MyType = "GPT"
                    });
                }
                myDiskInfo.Add(new MyDiskInfo()
                {
                    
                });
            }
            dgwDisk.DataSource = myDiskInfo;
        }
        public class MyDiskInfo
        {
            public long Size { get; set; }
            public string MyType { get; set; }
            public int ID { get; set; }
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
            if (txtDiskName.Text.Length == 1)
            {
                char targetLetter = char.Parse(txtDiskName.Text);
                var diskID = Convert.ToInt32(dgwDisk.Rows[dgwDisk.CurrentRow.Index].Cells["ID"].Value);
                diskTracker.CW_RestoreToFreshDisk(targetLetter, myBackupMetadata.Letter, myBackupMetadata.Version, diskID, myMain.myPath);
            }
            else
            {
                MessageBox.Show("Lütfen geçerli bir değer giriniz");
            }

        }

        private void btnCancel_Click(object sender, EventArgs e)
        {
            OpenChildForm(new FormRestore(myMain));
        }
    }
}
