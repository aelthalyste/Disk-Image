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
        public List<VolumeInformation> volumes;
        public DiskTracker diskTracker;
        public int type;
        //public class VolumeInformation
        //{
        //    public bool Checked { get; set; }
        //    public long Size { get; set; }
        //    public char Letter { get; set; }
        //    public int DiskID { get; set; }
        //    public char DiskType { get; set; }
        //    public bool Bootable { get; set; }
        //}
        public Main()
        {
            InitializeComponent();
        }

        private void btnAdd_Click(object sender, EventArgs e)
        { 
            List<DataGridViewRow> checkedColumn = new List<DataGridViewRow>();
            volumes = new List<VolumeInformation>();
            foreach (DataGridViewRow row in dataGridView1.Rows)
            {
                if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
                {
                    checkedColumn.Add(row);
                }             
            }
            myMessageBox = new MyMessageBox();
            foreach (var item in checkedColumn)
            {
                myMessageBox.volumeInformations.Add(new VolumeInformation() {Letter = Convert.ToChar(item.Cells["Letter"].Value)});
            }
            myMessageBox.MessageText = volumes.Count.ToString();
            myMessageBox.Show();
        }

        private void btnPath_Click(object sender, EventArgs e)
        {
            DialogResult result = folderBrowserDialog1.ShowDialog();
            if (result == DialogResult.OK)
            {
                string[] files = Directory.GetFiles(folderBrowserDialog1.SelectedPath);
                txtPath.Text = folderBrowserDialog1.SelectedPath;
                rtReport.Text = "Dosya konumu  "+folderBrowserDialog1.SelectedPath + "  olarak seçildi.";
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            //List<VolumeInformation> volumeInformations = new List<VolumeInformation>();
            //volumeInformations.Add(new VolumeInformation() { Checked = false, SizeMB = (uint)(1024L * 1024L * 1024L), DiskID = 12, DiskType = 'M', Letter = 'E', Bootable = true });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L * 1024L * 1024L, DiskID = 12, DiskType = 'M', Letter = 'E', Bootable = false });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L, DiskID = 15, DiskType = 'M', Letter = 'D', Bootable = false });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L, DiskID = 17, DiskType = 'M', Letter = 'C', Bootable = true });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 400L * 1024L, DiskID = 22, DiskType = 'M', Letter = 'C', Bootable = false });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L * 1024L, DiskID = 12, DiskType = 'M', Letter = 'E', Bootable = true });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L * 1024L * 1024L, DiskID = 12, DiskType = 'M', Letter = 'E', Bootable = false });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L, DiskID = 15, DiskType = 'M', Letter = 'D', Bootable = false });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L, DiskID = 17, DiskType = 'M', Letter = 'C', Bootable = true });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 400L * 1024L, DiskID = 22, DiskType = 'M', Letter = 'C', Bootable = false });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L * 1024L, DiskID = 12, DiskType = 'M', Letter = 'E', Bootable = true });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L * 1024L * 1024L, DiskID = 12, DiskType = 'M', Letter = 'E', Bootable = false });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L, DiskID = 15, DiskType = 'M', Letter = 'D', Bootable = false });
            //volumeInformations.Add(new VolumeInformation() {Checked = false, Size = 1024L * 1024L, DiskID = 17, DiskType = 'M', Letter = 'C', Bootable = true });
            //volumeInformations.Add(new VolumeInformation() { Checked = false, Size = 400L * 1024L, DiskID = 22, DiskType = 'M', Letter = 'C', Bootable = false });
            //diskTracker.
            dataGridView1.DataSource = volumeInformations;
            dataGridView1.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        }

        private void dataGridView1_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            var a = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells[0].Value;
            var b = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells[1].Value;
            var c = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells[2].Value;
            var d = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells[3].Value;
            var f = dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells[4].Value;
            rtReport.Text = "Size : " + a.ToString() + "\nLetter : " +b.ToString() + "\nDiskId : "+c.ToString()+ "\nDiskType : " + d.ToString()+ "\nBootable : " + f.ToString();
        }

        private void btnExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void btnExpand_Click(object sender, EventArgs e)
        {
            if (WindowState == FormWindowState.Normal)
                WindowState = FormWindowState.Maximized;
            else
                WindowState = FormWindowState.Normal;
        }

        private void btnMinimize_Click(object sender, EventArgs e)
        {
            WindowState = FormWindowState.Minimized;
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
    }
}
