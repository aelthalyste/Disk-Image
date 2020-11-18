using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
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
        private CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();

        private StatusInfo _statusInfo = new StatusInfo();

        private readonly IStatusInfoDal _statusInfoDal;

        public StatusesWindow(int chooseFlag, StatusInfo statusInfo, IStatusInfoDal statusInfoDal)
        {
            InitializeComponent();
            _statusInfoDal = statusInfoDal;
            _statusInfo = statusInfo;
            RefreshStatus(_cancellationTokenSource.Token);
            this.Closing += (sender, e) => _cancellationTokenSource.Cancel();

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

        public async void RefreshStatus(CancellationToken cancellationToken)
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                await Task.Delay(500);
                _statusInfo = _statusInfoDal.Get(x => x.Id == _statusInfo.Id);
                pbTotalDataProcessed.Maximum = _statusInfo.TotalDataProcessed;
                pbTotalDataProcessed.Value = _statusInfo.DataProcessed;
                txtLocalPercentage.Text = Math.Round((_statusInfo.DataProcessed * 100.0) / (_statusInfo.TotalDataProcessed),2).ToString() + "%";
                txtLocalFileName.Text = _statusInfo.FileName;
                txtLocalTime.Text = _statusInfo.TimeElapsed.ToString() + " ms"; // milisaniye
                txtLocalAverageDataRate.Text = Math.Round(_statusInfo.AverageDataRate, 2).ToString() + " MB/s";
                txtLocalDataProcessed.Text = _statusInfo.DataProcessed.ToString() + " byte"; //dönüş değerine bakılmalı byte, kb, mb, gb...
                txtLocalInstantDataRate.Text = Math.Round(_statusInfo.InstantDataRate, 2).ToString() + " MB/s"; //dönüş değerine bakılmalı byte, kb, mb, gb...
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
