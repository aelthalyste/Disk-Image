using LicenseKeyGenerator.Entities;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace LicenseKeyGenerator
{
    /// <summary>
    /// Interaction logic for KeyInfoWindow.xaml
    /// </summary>
    public partial class KeyInfoWindow : Window
    {
        public KeyInfoWindow(License license)
        {
            InitializeComponent();
            
            txtCustomerName.Text = license.CustomerName;
            txtAuthorizedPerson.Text = license.AuthorizedPerson;
            txtDealerName.Text = license.DealerName;
            txtEndDate.Text = license.SupportEndDate.ToString();
            txtVerificationKey.Text = license.UniqKey;
            txtLicenceKey.Text = license.Key;

            if (license.LicenseVersion == VersionType.SBS)
                txtVersion.Text = "SBS";
            else if(license.LicenseVersion == VersionType.Server)
                txtVersion.Text = "Server";
            else
                txtVersion.Text = "Workstation";
        }

        #region Title Bar

        private void btnMin_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void btnClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }

        private void btnNormal_Click(object sender, RoutedEventArgs e)
        {
            if (WindowState == WindowState.Maximized)
                WindowState = WindowState.Normal;
            else
                WindowState = WindowState.Maximized;
        }

        #endregion

        private void btnExportFile_Click(object sender, RoutedEventArgs e)
        {
            using (var dialog = new System.Windows.Forms.FolderBrowserDialog())
            {
                var result = dialog.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    ExportFile(dialog.SelectedPath, txtVerificationKey.Text, txtLicenceKey.Text);
                }
            }
        }

        private void ExportFile(string path, string uniqKey, string key)
        {
            string filePath = path + @"\" + uniqKey + ".nbkey";
            FileStream fileStream = new FileStream(filePath, FileMode.OpenOrCreate, FileAccess.Write);
            StreamWriter streamWriter = new StreamWriter(fileStream);
            streamWriter.Write(key);
            streamWriter.Flush();
            streamWriter.Close();
            fileStream.Close();
        }
    }
}
