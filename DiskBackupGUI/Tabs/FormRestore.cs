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

        public FormRestore(Main main)
        {
            myMain = main;
            InitializeComponent();
            diskTracker = new DiskTracker();
            
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

        private void btnDisc_Click(object sender, EventArgs e)
        {
            //myMain.RtReportWrite("Disk seçildi", false);
            //List<DataGridViewRow> checkedDgvVolume = new List<DataGridViewRow>();
            //foreach (DataGridViewRow row in dgwRestore.Rows)
            //{
            //    if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
            //    {
            //        checkedDgvVolume.Add(row);
            //    }
            //}
            OpenChildForm(new RestoreDisk(myMain));
        }

        private void btnVolume_Click(object sender, EventArgs e)
        {
            //myMain.RtReportWrite("Volume seçildi", false);
            //List<DataGridViewRow> checkedDgvVolume = new List<DataGridViewRow>();
            //foreach (DataGridViewRow row in dgwRestore.Rows)
            //{
            //    if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
            //    {
            //        checkedDgvVolume.Add(row);
            //    }
            //}
            OpenChildForm(new RestoreVolume(myMain));
        }

        private void dgwRestore_CellClick(object sender, DataGridViewCellEventArgs e)
        {

        }
    }
}
