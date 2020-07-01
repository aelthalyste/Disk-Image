using NarDIWrapper;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace DiskBackupGUI
{
    public partial class MyMessageBox : Form
    {
        public string MessageText;
        public List<VolumeInformation> volumeInformations;
        public MyMessageBox()
        {
            InitializeComponent();
        }

        private void MyMessageBox_Load(object sender, EventArgs e)
        {
            lblMessage.Text = MessageText;
        }

        private void btnOkay_Click(object sender, EventArgs e)
        {
            foreach (var item in volumeInformations)
            {

            }
            this.Close();
        }

        //panelden formu hareket ettirmek için : 
        [DllImport("user32.DLL", EntryPoint = "ReleaseCapture")]
        private extern static void ReleaseCapture();
        [DllImport("user32.DLL", EntryPoint = "SendMessage")]
        private extern static void SendMessage(System.IntPtr hWnd, int wMsg, int wParam, int lParam);

        private void lblMessage_MouseDown(object sender, MouseEventArgs e)
        {
            ReleaseCapture();
            SendMessage(this.Handle, 0x112, 0xf012, 0);
        }
    }
}
