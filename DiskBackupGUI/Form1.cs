using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using NarDIWrapper;
namespace DiskBackupGUI
{
    public partial class Form1 : Form
    {
        public class VolumeInformation
        {
            public long Size { get; set; }
            public char Letter { get; set; }
            public int DiskID { get; set; }
            public char DiskType { get; set; }
            public bool Bootable { get; set; }
        }
        public Form1()
        {
            InitializeComponent();
      DiskTracker t = new DiskTracker();
      t.GetVolumes();

        }

        private void btnAdd_Click(object sender, EventArgs e)
        {
            MessageBox.Show("aaaa");
        }

        private void btnPath_Click(object sender, EventArgs e)
        {
            DialogResult result = folderBrowserDialog1.ShowDialog();
            if (result == DialogResult.OK)
            {
                string[] files = Directory.GetFiles(folderBrowserDialog1.SelectedPath);
                txtPath.Text = folderBrowserDialog1.SelectedPath;
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            List<VolumeInformation> volumeInformations = new List<VolumeInformation>();
            volumeInformations.Add(new VolumeInformation() {Size = 1024L * 1024L * 1024L, DiskID = 12, DiskType = 'M', Letter='E',Bootable = false});
            volumeInformations.Add(new VolumeInformation() {Size = 1024L * 1024L * 1024L * 1024L, DiskID = 12, DiskType = 'M', Letter='E',Bootable = false});
            volumeInformations.Add(new VolumeInformation() {Size = 1024L * 1024L, DiskID = 15, DiskType = 'M', Letter='D',Bootable = false});
            volumeInformations.Add(new VolumeInformation() {Size = 1024L * 1024L, DiskID = 17, DiskType = 'M', Letter='C',Bootable = true});
            volumeInformations.Add(new VolumeInformation() {Size = 400L * 1024L, DiskID = 22, DiskType = 'M', Letter='C',Bootable = false });
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
            rtReport.Text = "Size : " + a.ToString() + "\nDiskId : "+b.ToString()+ "\nDiskType : " + c.ToString()+ "\nLetter : " + d.ToString()+ "\nBootable : " + f.ToString();
        }
    }
}
