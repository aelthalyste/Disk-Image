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
using DiskBackupGUI.Tabs;
using FontAwesome.Sharp;
using NarDIWrapper;
namespace DiskBackupGUI
{
    public partial class Main : Form
    {
        public List<MyVolumeInformation> volumes;
        public DiskTracker diskTracker;
        public int type;
        public string Path;

        public static Color activeColor = Color.FromArgb(37, 36, 81);

        private Form currentChildForm;
        private Panel leftBorderBtn;
        private IconButton currentBtn;

        public class MyVolumeInformation
        {
            public bool Checked { get; set; }
            public long Size { get; set; }
            public char Letter { get; set; }
            public int DiskID { get; set; }
            public string DiskType { get; set; }
            public byte Bootable { get; set; }
        }

        private void Main_Load(object sender, EventArgs e)
        {
        }

        public Main()
        {
            InitializeComponent();
            diskTracker = new DiskTracker();
            volumes = new List<MyVolumeInformation>();

            leftBorderBtn = new Panel();
            leftBorderBtn.Size = new Size(7, 60);
            panelMenu.Controls.Add(leftBorderBtn);

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
            dataGridView1.DataSource = volumes;
            dataGridView1.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
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

        public void RtReportWrite(string myText, bool add)
        {
            if (add == true)
            {
                rtReport.Text += myText;
            }
            else
            {
                rtReport.Text = myText;
            }
        }

        private void ActiveButton(object senderBtn, Color color)
        {
            if (senderBtn != null)
            {
                DisableButton();
                //Button
                currentBtn = (IconButton)senderBtn;
                currentBtn.BackColor = Color.FromArgb(0, 150, 199);
                currentBtn.ForeColor = color;
                currentBtn.TextAlign = ContentAlignment.MiddleCenter;
                currentBtn.IconColor = color;
                currentBtn.TextImageRelation = TextImageRelation.TextBeforeImage;
                currentBtn.ImageAlign = ContentAlignment.MiddleRight;
                //Left Border Button
                leftBorderBtn.BackColor = color;
                leftBorderBtn.Location = new Point(0, currentBtn.Location.Y);
                leftBorderBtn.Visible = true;
                leftBorderBtn.BringToFront();
            }
        }

        private void DisableButton()
        {
            if (currentBtn != null)
            {
                currentBtn.BackColor = Color.FromArgb(2, 62, 138);
                currentBtn.ForeColor = Color.Gainsboro;
                currentBtn.TextAlign = ContentAlignment.MiddleLeft;
                currentBtn.IconColor = Color.Gainsboro;
                currentBtn.TextImageRelation = TextImageRelation.ImageBeforeText;
                currentBtn.ImageAlign = ContentAlignment.MiddleCenter;
            }
        }

        private async Task BackupThreadAsync(List<DataGridViewRow> checkedColumn, int typeParam)
        {
            StreamInfo str = new StreamInfo();
            rtReport.Text = checkedColumn.Count.ToString() + " veri seçildi";
            int bufferSize = 64 * 1024 * 1024;
            byte[] buffer = new byte[bufferSize];
            long readSoFar = 0;
            bool result = false;
            
            foreach (var item in checkedColumn)
            {
                if (diskTracker.CW_SetupStream((char)item.Cells["Letter"].Value, typeParam, str))
                {
                    rtReport.Text += $"\n{item.Cells["Letter"].Value.ToString()} Incremental eklendi";
                    MessageBox.Show("Setup Done");
                    unsafe
                    {
                        fixed (byte* BAddr = &buffer[0])
                        {
                            FileStream file = File.Create(Path + str.FileName); //kontrol etme işlemine bak
                            while (diskTracker.CW_ReadStream(BAddr, bufferSize))
                            {
                                readSoFar += bufferSize;
                                file.Write(buffer, 0, bufferSize);

                                MessageBox.Show("Read Done");
                            }
                            if (readSoFar != str.ClusterCount * str.ClusterSize)
                            {
                                long ReadRemaining = (long)(str.ClusterCount * str.ClusterSize - readSoFar);
                                if (ReadRemaining <= bufferSize)
                                {
                                    file.Write(buffer, 0, (int)ReadRemaining);
                                    result = true;
                                    MessageBox.Show("Check Done");
                                }
                                else
                                {
                                    MessageBox.Show("Error");
                                    rtReport.Text = "Error";
                                }
                            }
                            diskTracker.CW_TerminateBackup(result);
                        }
                    }
                }
                MessageBox.Show("Done");
            }
        }

        private void dataGridView1_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            RtReportWrite("Size : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Size"].Value.ToString()
                       + "\nLetter : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Letter"].Value.ToString()
                       + "\nDiskId : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["DiskId"].Value.ToString()
                       + "\nDiskType : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["DiskType"].Value.ToString()
                       + "\nBootable : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Bootable"].Value.ToString(), false);
        }

        private void btnDifferential_Click(object sender, EventArgs e)
        {
            int typeParam = 0;
            List<DataGridViewRow> checkedColumn = new List<DataGridViewRow>();
            foreach (DataGridViewRow row in dataGridView1.Rows)
            {
                if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
                {
                    checkedColumn.Add(row);
                }
            }
            Task task = BackupThreadAsync(checkedColumn, typeParam);
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
            Task task = BackupThreadAsync(checkedColumn, typeParam);
        }

        private void btnCancel_Click(object sender, EventArgs e)
        {
            //thread kapat
            PBMain.Value = 0;
            List<DataGridViewRow> checkedColumn = new List<DataGridViewRow>();
            foreach (DataGridViewRow row in dataGridView1.Rows)
            {
                if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
                {
                    Convert.ToBoolean(row.Cells["Checked"].Value = false);
                    checkedColumn.Add(row);
                }
            }
            rtReport.Text = "Şeçili satırlar kaldırıldı";
        }

        private void btnPath_Click(object sender, EventArgs e)
        {
            folderBrowserDialog1.ShowDialog();
            Path = folderBrowserDialog1.SelectedPath;
        }

        private void btnHome_Click(object sender, EventArgs e)
        {
            if (currentChildForm != null)
            {
                currentChildForm.Close();
            }
            DisableButton();
        }

        private void btnAdd_Click(object sender, EventArgs e)
        {
            ActiveButton(sender, activeColor);
            OpenChildForm(new FormAdd());
        }

        private void btnRestore_Click(object sender, EventArgs e)
        {
            ActiveButton(sender, activeColor);
            OpenChildForm(new FormRestore(this));
        }

        private void btnExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void btnMinimize_Click(object sender, EventArgs e)
        {
            WindowState = FormWindowState.Minimized;
        }

        //panelden formu hareket ettirmek için: 
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
