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
using static DiskBackupGUI.Main;

namespace DiskBackupGUI.Tabs.RestoreTabs
{
    public partial class RestoreVolume : Form
    {
        Main myMain;
        FormRestore formRestore;
        FormRestore.MyBackupMetadata myBackupMetadata;
        public DiskTracker diskTracker;
        private int choseVolume = 0;
        public List<MyVolumeInformation> volumes;
        private Form currentChildForm;

        public RestoreVolume(Main main, FormRestore.MyBackupMetadata myBackup)
        {
            //volume listesini tekrar çağır mainden gelmesi sıkıntı olabilir
            InitializeComponent();
            formRestore = new FormRestore(myMain);
            myMain = main;
            myBackupMetadata = myBackup;
            diskTracker = new DiskTracker();
            volumes = new List<MyVolumeInformation>();

            foreach (var item in diskTracker.CW_GetVolumes())
            {
                if (item.DiskType == 'R')
                {
                    volumes.Add(new MyVolumeInformation()
                    {
                        Checked = false,
                        Bootable = item.Bootable,
                        DiskID = item.DiskID,
                        Letter = (char)item.Letter,
                        Size = (long)item.Size,
                        DiskType = "RAW",
                    });
                }
                else if (item.DiskType == 'M')
                {
                    volumes.Add(new MyVolumeInformation()
                    {
                        Checked = false,
                        Bootable = item.Bootable,
                        DiskID = item.DiskID,
                        Letter = (char)item.Letter,
                        Size = (long)item.Size,
                        DiskType = "MBR",
                    });
                }
                else if (item.DiskType == 'G')
                {
                    volumes.Add(new MyVolumeInformation()
                    {
                        Checked = false,
                        Bootable = item.Bootable,
                        DiskID = item.DiskID,
                        Letter = (char)item.Letter,
                        Size = (long)item.Size,
                        DiskType = "GPT"
                    });
                }
            }
        }

        private void RestoreVolume_Load(object sender, EventArgs e)
        {
            dgwVolume.DataSource = volumes;
            dgwVolume.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;

            myMain.RtReportWrite("Letter : " + myBackupMetadata.Letter.ToString()
                + "\nBackupType : " + myBackupMetadata.BackupType.ToString()
                + "\nVersion  : " + myBackupMetadata.Version.ToString()
                + "\nOSVolume : " + myBackupMetadata.OSVolume.ToString()
                + "\nDiskType : " + myBackupMetadata.DiskType.ToString(), false);
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

        private void dgwVolume_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            myMain.RtReportWrite("Size : " + dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["Size"].Value.ToString()
                        + "\nLetter : " + dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["Letter"].Value.ToString()
                        + "\nDiskId : " + dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["DiskId"].Value.ToString()
                        + "\nDiskType : " + dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["DiskType"].Value.ToString()
                        + "\nBootable : " + dgwVolume.Rows[dgwVolume.CurrentRow.Index].Cells["Bootable"].Value.ToString(), false);
        }

        private void btnRestore_Click(object sender, EventArgs e)
        {
          //aşşağıdaki koda ihtiyaç olmayabilir... 
          //**************************************************
          //**************************************************
          //              BURAYI TEKRAR DÜŞÜN
          //**************************************************
          //**************************************************

          List<DataGridViewRow> checkedDgvVolume = new List<DataGridViewRow>();
          foreach (DataGridViewRow row in dgwVolume.Rows)
          {
              if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
              {
                  checkedDgvVolume.Add(row);
              }
          }
        }

        private void btnCancel_Click(object sender, EventArgs e)
        {
            OpenChildForm(new FormRestore(myMain));
        }
    }
}
