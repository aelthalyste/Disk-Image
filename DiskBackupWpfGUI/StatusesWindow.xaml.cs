using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
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

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for StatusesWindow.xaml
    /// </summary>
    public partial class StatusesWindow : Window
    {
        public StatusesWindow(int chooseFlag)
        {
            InitializeComponent();
            // 0 yedekleme durumu, 1 geri yükleme
            if (chooseFlag == 0)
            {
                stackLocalTaskName.Visibility = Visibility.Visible;
                txtLocalTaskName.Visibility = Visibility.Visible;
                stackCloudTaskName.Visibility = Visibility.Visible;
                txtCloudTaskName.Visibility = Visibility.Visible;
                txtTitleBar.Text = Resources["backupStatus"].ToString();
            }
            else
            {
                stackLocalTaskName.Visibility = Visibility.Visible;
                txtLocalTaskName.Visibility = Visibility.Visible;
                stackCloudTaskName.Visibility = Visibility.Visible;
                txtCloudTaskName.Visibility = Visibility.Visible;
                stackLocalZip.Visibility = Visibility.Collapsed;
                txtLocalZip.Visibility = Visibility.Collapsed;
                stackCloudZip.Visibility = Visibility.Collapsed;
                txtCloudZip.Visibility = Visibility.Collapsed;
                txtTitleBar.Text = Resources["restoreStatus"].ToString();
            }
        }

        public StatusesWindow(int chooseFlag, StatusInfo statusInfo)
        {
            InitializeComponent();
            // 0 yedekleme durumu, 1 geri yükleme
            if (chooseFlag == 0)
            {
                stackLocalTaskName.Visibility = Visibility.Visible;
                txtLocalTaskName.Visibility = Visibility.Visible;
                stackCloudTaskName.Visibility = Visibility.Visible;
                txtCloudTaskName.Visibility = Visibility.Visible;
                txtTitleBar.Text = Resources["backupStatus"].ToString();

                txtLocalTaskName.Text = statusInfo.TaskName;
                txtLocalFileName.Text = statusInfo.FileName;
                txtLocalTime.Text = statusInfo.TimeElapsed.ToString() + " ms"; // milisaniye
                txtLocalAverageDataRate.Text = statusInfo.AverageDataRate.ToString() + " MB/s";
                txtLocalDataProcessed.Text = statusInfo.DataProcessed.ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                txtLocalInstantDataRate.Text = statusInfo.InstantDataRate.ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                txtLocalSourceObje.Text = statusInfo.SourceObje;

                pbTotalDataProcessed.Maximum = statusInfo.TotalDataProcessed;
                pbTotalDataProcessed.Value = statusInfo.DataProcessed;
                MessageBox.Show($"İşlenmesi gereken veri: {statusInfo.TotalDataProcessed} - İşlenen veri: {statusInfo.DataProcessed}");
            }
            else
            {
                stackLocalTaskName.Visibility = Visibility.Visible;
                txtLocalTaskName.Visibility = Visibility.Visible;
                stackCloudTaskName.Visibility = Visibility.Visible;
                txtCloudTaskName.Visibility = Visibility.Visible;
                stackLocalZip.Visibility = Visibility.Collapsed;
                txtLocalZip.Visibility = Visibility.Collapsed;
                stackCloudZip.Visibility = Visibility.Collapsed;
                txtCloudZip.Visibility = Visibility.Collapsed;
                txtTitleBar.Text = Resources["restoreStatus"].ToString();
            }
        }

        #region Title Bar
        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }

        private void btnTaskStatusMin_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;

        }

        private void btnTaskStatusClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        #endregion
    }
}
