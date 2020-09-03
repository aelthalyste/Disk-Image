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

namespace DiskBackupGUI.Tabs
{
    public partial class FormAdd : Form
    {
        Main myMain;
        public FormAdd(Main main)
        {
            //serilization edilen görevleri göstermek ve fail olan görevleri göstermek için kullanılacak form 
            InitializeComponent();
            myMain = main;
            FileRead();
        }

        private void FileRead()
        {
            string fileName = @"SystemLog.txt";
            StreamReader sr = new StreamReader(fileName);
            rtxtLog.Text = sr.ReadToEnd();
        }

        private void btnRefresh_Click(object sender, EventArgs e)
        {
            FileRead();
            myMain.RtReportWrite("Dosya başarıyla okunup yenilendi.", false);
        }
    }
}
