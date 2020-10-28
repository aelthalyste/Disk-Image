using System;
using System.Collections.Generic;
using System.Collections.Specialized;
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
using Quartz;
using Quartz.Impl;

namespace DiskBackupGUI
{
    public partial class Main : Form
    {
        public List<MyVolumeInformation> volumes;
        public DiskTracker diskTracker;
        public int BackupNum = 2;
        public int type;
        public string myPath = "";
        public Dictionary<string, List<char>> taskParams = new Dictionary<string, List<char>>();
        StreamWriter writer;
        public static Main Instance { get; private set; }

        public static Color activeColor = Color.FromArgb(37, 36, 81);

        private Form currentChildForm;
        private Panel leftBorderBtn;
        private IconButton currentBtn;
        private IScheduler scheduler;

        public async Task InitSheduler()
        {
            StdSchedulerFactory factory = new StdSchedulerFactory();
            scheduler = await factory.GetScheduler();
            await scheduler.Start();
        }

        //c++ ile dönen değişkenlerin c# ile uyumlu olması için oluşturulan class
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
            PathLoader();
        }

        public Main()
        {
            InitializeComponent();
            diskTracker = new DiskTracker();
            volumes = new List<MyVolumeInformation>();

            if (!diskTracker.CW_InitTracker())
            {
                MessageBox.Show("Driver initialize edilemedi");
            }

            string fileName = @"eyup_path.txt";
            StreamReader sr = new StreamReader(fileName);
            string lastPath = sr.ReadToEnd();
            sr.Close();

            if (lastPath != "")
            {
                myPath = lastPath;
                lblPath.Text = myPath;
            }
            else
            {
                lblPath.Text = "Dosya yolu yok!!";
                MessageBox.Show("Seçili bir dosya yolu bulunmamakta");
            }

            leftBorderBtn = new Panel();
            leftBorderBtn.Size = new Size(7, 60);
            panelMenu.Controls.Add(leftBorderBtn);

            //formda gösterim anında kısaltma "R" yerine tam adını yazması için "Raw" 
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
            Instance = this;
            Task.Run(async () =>
            {
                await InitSheduler();
            }).Wait();

        }

        //panelMain içerisine diğer formları yerleştirmek için kullanılan method
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

        //diğer form ekranlarından aşşağıdaki textbox'a yazı yazmayı sağlayan method
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

        //main formda lblPath objesini değişiklikten sonra yenilemek için
        private void PathLoader()
        {
            if (myPath != "")
            {
                lblPath.Text = myPath;
            }
            else
            {
                lblPath.Text = "Dosya yolu yok!!";
            }
        }

        //aktif olan ve içinde bulunduğumuz tab'ın belirginleşmesi için
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

        //aktiflikten çıkan butonun eski haline dönmesi için
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

        //scheduler yapıldığı için bu method şuanlık kullanılmıyor.
        //public async Task BackupThreadAsync(List<DataGridViewRow> checkedColumn, int typeParam)
        //{
        //    StreamInfo str = new StreamInfo();
        //    rtReport.Text = checkedColumn.Count.ToString() + " veri seçildi";
        //    int bufferSize = 64 * 1024 * 1024;
        //    byte[] buffer = new byte[bufferSize];
        //    long BytesReadSoFar = 0;
        //    bool result = false;
        //    int Read = 0;
        //    foreach (var item in checkedColumn)
        //    {
        //        if (diskTracker.CW_SetupStream((char)item.Cells["Letter"].Value, typeParam, str))
        //        {
        //            rtReport.Text += $"\n{item.Cells["Letter"].Value.ToString()} Incremental eklendi";
        //            MessageBox.Show("Setup Done");
        //            unsafe
        //            {
        //                fixed (byte* BAddr = &buffer[0])
        //                {
        //                    FileStream file = File.Create(Main.Instance.myPath + str.FileName); //kontrol etme işlemine bak
        //                    while (true)
        //                    {
        //                        Read = diskTracker.CW_ReadStream(BAddr, bufferSize);
        //                        if (Read == 0)
        //                            break;
        //                        file.Write(buffer, 0, Read);
        //                        BytesReadSoFar += Read;
        //                    }

        //                    result = (long)str.ClusterCount * (long)str.ClusterSize == BytesReadSoFar;
        //                    diskTracker.CW_TerminateBackup(result);
        //                }
        //            }
        //        }
        //        MessageBox.Show("Done");
        //    }
        //}

        //bir satırın üzerindeyken aşşağıda bulunan log ekranına satırdaki bilgileri girmesi için çalışan event
        private void dataGridView1_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            //rtReport textbox'una seçili row bilgilerini yazar
            RtReportWrite("Size : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Size"].Value.ToString()
                       + "\nLetter : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Letter"].Value.ToString()
                       + "\nDiskId : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["DiskId"].Value.ToString()
                       + "\nDiskType : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["DiskType"].Value.ToString()
                       + "\nBootable : " + dataGridView1.Rows[dataGridView1.CurrentRow.Index].Cells["Bootable"].Value.ToString(), false);
        }

        //differential backup metodunu çalıştıran method
        private void btnDifferential_Click(object sender, EventArgs e)
        {
            List<char> checkedColumn = new List<char>();
            foreach (DataGridViewRow row in dataGridView1.Rows)
            {
                if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
                {
                    checkedColumn.Add((char)row.Cells["Letter"].Value);
                }
            }
            int diffNum = 0;
            BackupNum = 0;
            MyMessageBox myMessageBox = new MyMessageBox(diffNum);
            myMessageBox.MyScheduler = scheduler;
            myMessageBox.Letters = checkedColumn;
            myMessageBox.Show();
        }

        //incremental backup metodunu çalıştıran method
        private void btnIncremental_Click(object sender, EventArgs e)
        {
            List<char> checkedColumn = new List<char>();
            foreach (DataGridViewRow row in dataGridView1.Rows)
            {
                if (Convert.ToBoolean(row.Cells["Checked"].Value) == true)
                {
                    checkedColumn.Add((char)row.Cells["Letter"].Value);
                }
            }
            int incNum = 1;
            BackupNum = 1;
            MyMessageBox myMessageBox = new MyMessageBox(incNum);
            myMessageBox.MyScheduler = scheduler;
            myMessageBox.Letters = checkedColumn;
            myMessageBox.Show();
        }

        //checked olan satırları kapatmak için
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

        //path değişkenine global'de ihtiyaç olduğu için soldaki menüye eklemeyi düşün.
        private void btnPath_Click(object sender, EventArgs e)
        {
            folderBrowserDialog1.ShowDialog();
            myPath = folderBrowserDialog1.SelectedPath + @"\";
            string fileName = @"eyup_path.txt";
            StreamWriter pathWriter = new StreamWriter(fileName);
            pathWriter.Write(myPath);
            pathWriter.Close();
            PathLoader();
        }

        //ana ekrana dönme methodu
        private void btnHome_Click(object sender, EventArgs e)
        {
            if (currentChildForm != null)
            {
                currentChildForm.Close();
            }
            DisableButton();
        }

        //log tab açma methodu
        private void btnLog_Click(object sender, EventArgs e)
        {
            ActiveButton(sender, activeColor);
            OpenChildForm(new FormAdd(this));
        }

        //restore tab açma methodu
        private void btnRestore_Click(object sender, EventArgs e)
        {
            ActiveButton(sender, activeColor);
            OpenChildForm(new FormRestore(this));
        }

        //formu kapatma butonu
        private void btnExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        //formu minimize hale getirip görev çubuğu simgesine dönmesini sağlayan kısım
        private void btnMinimize_Click(object sender, EventArgs e)
        {
            WindowState = FormWindowState.Minimized;
        }

        //panelden formu hareket ettirmek için: 
        #region Form Hareket

        [DllImport("user32.DLL", EntryPoint = "ReleaseCapture")]
        private extern static void ReleaseCapture();
        [DllImport("user32.DLL", EntryPoint = "SendMessage")]
        private extern static void SendMessage(System.IntPtr hWnd, int wMsg, int wParam, int lParam);

        //formun üst kısmındaki panelden formu hareket ettirmek için
        private void panel4_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }

        //formun altındaki textbox'tan formu hareket ettirmek için
        private void rtReport_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }

        private void panelMenu_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }
        #endregion
    }
}
