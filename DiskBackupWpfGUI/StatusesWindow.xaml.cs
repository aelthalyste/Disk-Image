﻿using DiskBackup.DataAccess.Abstract;
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
                txtLocalTime.Text = FormatMilliseconds(TimeSpan.FromMilliseconds(statusInfo.TimeElapsed)); // milisaniye
                txtLocalAverageDataRate.Text = statusInfo.AverageDataRate.ToString() + " MB/s";
                txtLocalDataProcessed.Text = FormatBytes(statusInfo.DataProcessed).ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                txtLocalInstantDataRate.Text = statusInfo.InstantDataRate.ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                if (statusInfo.SourceObje.Contains("-"))
                {
                    var source = statusInfo.SourceObje.Split('-');
                    txtLocalSourceObje.Text = source[0];
                    txtSourceSingle.Text = source[1];
                }
                else
                {
                    txtLocalSourceObje.Text = statusInfo.SourceObje;
                    txtSourceSingle.Text = statusInfo.SourceObje;
                }

                pbTotalDataProcessed.Maximum = statusInfo.TotalDataProcessed;
                pbTotalDataProcessed.Value = statusInfo.DataProcessed;
            }
            else
            {
                stackLocalTaskName.Visibility = Visibility.Visible;
                txtLocalTaskName.Visibility = Visibility.Visible;
                stackCloudTaskName.Visibility = Visibility.Visible;
                txtCloudTaskName.Visibility = Visibility.Visible;
                //stackLocalZip.Visibility = Visibility.Collapsed;
                //txtLocalZip.Visibility = Visibility.Collapsed;
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
                txtLocalPercentage.Text = Math.Round((_statusInfo.DataProcessed * 100.0) / (_statusInfo.TotalDataProcessed), 2).ToString() + "%";
                txtLocalFileName.Text = _statusInfo.FileName;
                txtLocalTime.Text = FormatMilliseconds(TimeSpan.FromMilliseconds(_statusInfo.TimeElapsed)); // milisaniye
                txtLocalAverageDataRate.Text = Math.Round(_statusInfo.AverageDataRate, 2).ToString() + " MB/s";
                txtLocalDataProcessed.Text = FormatBytes(_statusInfo.DataProcessed).ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                txtLocalInstantDataRate.Text = Math.Round(_statusInfo.InstantDataRate, 2).ToString() + " MB/s"; //dönüş değerine bakılmalı byte, kb, mb, gb...
                if (_statusInfo.SourceObje.Contains("-"))
                {
                    var source = _statusInfo.SourceObje.Split('-');
                    txtLocalSourceObje.Text = source[0];
                    txtSourceSingle.Text = source[1];
                }
                else
                {
                    txtLocalSourceObje.Text = _statusInfo.SourceObje;
                    txtSourceSingle.Text = _statusInfo.SourceObje;
                }
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

        private static string FormatBytes(long bytes)
        {
            string[] Suffix = { "B", "KB", "MB", "GB", "TB" };
            int i;
            double dblSByte = bytes;
            for (i = 0; i < Suffix.Length && bytes >= 1024; i++, bytes /= 1024)
            {
                dblSByte = bytes / 1024.0;
            }

            return ($"{dblSByte:0.##} {Suffix[i]}");
        }

        public static string FormatMilliseconds(TimeSpan obj)
        {
            StringBuilder sb = new StringBuilder();
            if (obj.Hours != 0)
            {
                sb.Append(obj.Hours);
                sb.Append(" ");
                sb.Append("s");
                sb.Append(" ");
            }
            if (obj.Minutes != 0 || sb.Length != 0)
            {
                sb.Append(obj.Minutes);
                sb.Append(" ");
                sb.Append("dk");
                sb.Append(" ");
            }
            if (obj.Seconds != 0 || sb.Length != 0)
            {
                sb.Append(obj.Seconds);
                sb.Append(" ");
                sb.Append("sn");
                sb.Append(" ");
            }
            if (obj.Milliseconds != 0 || sb.Length != 0)
            {
                sb.Append(obj.Milliseconds);
                sb.Append(" ");
                sb.Append("ms");
                sb.Append(" ");
            }
            if (sb.Length == 0)
            {
                sb.Append(0);
                sb.Append(" ");
                sb.Append("ms");
            }
            return sb.ToString();
        }

    }
}
