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
using NarDIWrapper;
namespace DiskBackupGUI
{
    public partial class Main : Form
    {
        MyMessageBox myMessageBox;
        public List<MyVolumeInformation> volumes;
        public DiskTracker diskTracker;
        public int type;
        public class MyVolumeInformation
        {
            public bool Checked { get; set; }
            public long Size { get; set; }
            public char Letter { get; set; }
            public int DiskID { get; set; }
            public char DiskType { get; set; }
            public byte Bootable { get; set; }
        }
        public Main()
        {
            InitializeComponent();
            diskTracker = new DiskTracker();
            volumes = new List<MyVolumeInformation>();

            foreach (var item in diskTracker.CW_GetVolumes())
            {
                volumes.Add(new MyVolumeInformation()
                {
                    Checked = false,
                    Bootable = item.Bootable,
                    DiskID = item.DiskID,
                    DiskType = (char)item.DiskType,
                    Letter = (char)item.Letter,
                    Size = (long)item.Size
                });
            }
            dataGridView1.DataSource = volumes;
            dataGridView1.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        }

        private void btnAdd_Click(object sender, EventArgs e)
        {
        }

        private void btnPath_Click(object sender, EventArgs e)
        {
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        //panelden formu hareket ettirmek için : 
        [DllImport("user32.DLL", EntryPoint = "ReleaseCapture")]
        private extern static void ReleaseCapture();
        [DllImport("user32.DLL", EntryPoint = "SendMessage")]
        private extern static void SendMessage(System.IntPtr hWnd, int wMsg, int wParam, int lParam);
        private void panel4_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }

        private void dataGridView1_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            var a = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Size"].Value;
            var b = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Letter"].Value;
            var c = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["DiskId"].Value;
            var d = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["DiskType"].Value;
            var f = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Bootable"].Value;
            rtReport.Text = "Size : " + a.ToString() + "\nLetter : " + b.ToString() + "\nDiskId : " + c.ToString() + "\nDiskType : " + d.ToString() + "\nBootable : " + f.ToString();
        }

        private void btnExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void btnMinimize_Click(object sender, EventArgs e)
        {
            WindowState = FormWindowState.Minimized;
        }

        private void btnIncremental_Click(object sender, EventArgs e)
        {
            int typeParam = 1;
            List<DataGridViewRow> checkedColumn = new List<DataGridViewRow>();
            foreach (DataGridViewRow row in dataGridView1.Rows)
            {
                if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
                {
                    checkedColumn.Add(row);
                }
            }
            rtReport.Text = checkedColumn.Count.ToString() + " veri seçildi";

            foreach (var item in checkedColumn)
            {
                if (diskTracker.CW_AddToTrack((char)item.Cells["Letter"].Value, typeParam))
                {
                    rtReport.Text += $"\n{item.Cells["Letter"].Value.ToString()} eklendi";
                }
            }
        }
    }
}
