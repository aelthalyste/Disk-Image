using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackupWpfGUI.Utils;
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

        private StatusInfo _statusInfo;
        private ActivityLog _activityLog;

        private readonly IStatusInfoDal _statusInfoDal;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IConfigurationDataDal _configurationDataDal;

        private int _taskId = 0;

        public StatusesWindow(int chooseFlag, TaskInfo taskInfo, IStatusInfoDal statusInfoDal, ITaskInfoDal taskInfoDal, IConfigurationDataDal configurationDataDal)
        {
            InitializeComponent();
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;
            _taskId = taskInfo.Id;
            _statusInfo = taskInfo.StatusInfo;
            RefreshStatus(_cancellationTokenSource.Token);
            this.Closing += (sender, e) => _cancellationTokenSource.Cancel();
            txtLastStatus.Text = taskInfo.StrStatus;

            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            // 0 yedekleme durumu, 1 geri yükleme
            if (chooseFlag == 0)
            {
                txtTitleBar.Text = Resources["backupStatus"].ToString();

                txtLocalTaskName.Text = _statusInfo.TaskName;
                txtCloudTaskName.Text = _statusInfo.TaskName;
                txtLocalFileName.Text = _statusInfo.FileName;
                txtCloudFileName.Text = _statusInfo.FileName;
                txtLocalTime.Text = FormatMilliseconds(TimeSpan.FromMilliseconds(_statusInfo.TimeElapsed)); // milisaniye
                txtLocalAverageDataRate.Text = _statusInfo.AverageDataRate.ToString() + " MB/s";
                txtLocalDataProcessed.Text = FormatBytes(_statusInfo.DataProcessed).ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                txtLocalInstantDataRate.Text = _statusInfo.InstantDataRate.ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                if (_statusInfo.SourceObje.Contains("-"))
                {
                    var source = _statusInfo.SourceObje.Split('-');
                    txtLocalSourceObje.Text = source[0];
                    txtCloudSourceObje.Text = source[0];
                    txtSourceSingle.Text = source[1];
                }
                else
                {
                    txtLocalSourceObje.Text = _statusInfo.SourceObje;
                    txtCloudSourceObje.Text = _statusInfo.SourceObje;
                    txtSourceSingle.Text = _statusInfo.SourceObje;
                }

                pbTotalDataProcessed.Maximum = _statusInfo.TotalDataProcessed;
                pbTotalDataProcessed.Value = _statusInfo.DataProcessed;
            }
            else
            {
                //stackLocalZip.Visibility = Visibility.Collapsed;
                //txtLocalZip.Visibility = Visibility.Collapsed;
                //stackCloudZip.Visibility = Visibility.Collapsed;
                //txtCloudZip.Visibility = Visibility.Collapsed;
                txtTitleBar.Text = Resources["restoreStatus"].ToString();
            }
        }

        public StatusesWindow(int chooseFlag, ActivityLog activityLog, IStatusInfoDal statusInfoDal, IConfigurationDataDal configurationDataDal)
        {
            InitializeComponent();
            _statusInfoDal = statusInfoDal;
            StatusInfo statusInfo = _statusInfoDal.Get(x => x.Id == activityLog.StatusInfoId);
            _statusInfo = statusInfo;
            _activityLog = activityLog;
            RefreshStatus(_cancellationTokenSource.Token);
            this.Closing += (sender, e) => _cancellationTokenSource.Cancel();
            txtLastStatus.Text = activityLog.StatusInfo.StrStatus;

            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            // 0 yedekleme durumu, 1 geri yükleme
            if (chooseFlag == 0)
            {
                txtTitleBar.Text = Resources["backupStatus"].ToString();

                txtLocalTaskName.Text = statusInfo.TaskName;
                txtCloudTaskName.Text = statusInfo.TaskName;
                txtLocalFileName.Text = statusInfo.FileName;
                txtCloudFileName.Text = statusInfo.FileName;
                txtLocalTime.Text = FormatMilliseconds(TimeSpan.FromMilliseconds(statusInfo.TimeElapsed)); // milisaniye
                txtLocalAverageDataRate.Text = statusInfo.AverageDataRate.ToString() + " MB/s";
                txtLocalDataProcessed.Text = FormatBytes(statusInfo.DataProcessed).ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                txtLocalInstantDataRate.Text = statusInfo.InstantDataRate.ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                if (statusInfo.SourceObje.Contains("-"))
                {
                    var source = statusInfo.SourceObje.Split('-');
                    txtLocalSourceObje.Text = source[0];
                    txtCloudSourceObje.Text = source[0];
                    txtSourceSingle.Text = source[1];
                }
                else
                {
                    txtLocalSourceObje.Text = statusInfo.SourceObje;
                    txtCloudSourceObje.Text = statusInfo.SourceObje;
                    txtSourceSingle.Text = statusInfo.SourceObje;
                }

                pbTotalDataProcessed.Maximum = statusInfo.TotalDataProcessed;
                pbTotalDataProcessed.Value = statusInfo.DataProcessed;
            }
            else
            {
                //stackLocalZip.Visibility = Visibility.Collapsed;
                //txtLocalZip.Visibility = Visibility.Collapsed;
                //stackCloudZip.Visibility = Visibility.Collapsed;
                //txtCloudZip.Visibility = Visibility.Collapsed;
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
                if (double.IsNaN(Math.Round((_statusInfo.DataProcessed * 100.0) / (_statusInfo.TotalDataProcessed), 2))) 
                    txtLocalPercentage.Text = "0%"; //TO DO NaN yakalandığında buraya 0 dışında bir şey girilmek istenir mi?
                else
                    txtLocalPercentage.Text = Math.Round((_statusInfo.DataProcessed * 100.0) / (_statusInfo.TotalDataProcessed), 2).ToString() + "%";
                txtLocalFileName.Text = _statusInfo.FileName;
                txtCloudFileName.Text = _statusInfo.FileName;
                txtLocalTime.Text = FormatMilliseconds(TimeSpan.FromMilliseconds(_statusInfo.TimeElapsed)); // milisaniye
                txtLocalAverageDataRate.Text = Math.Round(_statusInfo.AverageDataRate, 2).ToString() + " MB/s";
                txtLocalDataProcessed.Text = FormatBytes(_statusInfo.DataProcessed).ToString(); //dönüş değerine bakılmalı byte, kb, mb, gb...
                txtLocalInstantDataRate.Text = Math.Round(_statusInfo.InstantDataRate, 2).ToString() + " MB/s"; //dönüş değerine bakılmalı byte, kb, mb, gb...
                if (_statusInfo.SourceObje.Contains("-"))
                {
                    var source = _statusInfo.SourceObje.Split('-');
                    txtLocalSourceObje.Text = source[0];
                    txtCloudSourceObje.Text = source[0];
                    txtSourceSingle.Text = source[1];
                }
                else
                {
                    txtLocalSourceObje.Text = _statusInfo.SourceObje;
                    txtCloudSourceObje.Text = _statusInfo.SourceObje;
                    txtSourceSingle.Text = _statusInfo.SourceObje;
                }

                if (_taskId != 0)
                {
                    var resultTask = _taskInfoDal.Get(x => x.Id == _taskId);
                    if (resultTask.Status == TaskStatusType.Ready)
                    {
                        //status infodan alınacak
                        var statusInfo = _statusInfoDal.Get(x => x.Id == resultTask.StatusInfoId);
                        txtLastStatus.Text = Resources[statusInfo.Status.ToString()].ToString();
                    }
                    else
                        txtLastStatus.Text = Resources[resultTask.Status.ToString()].ToString();
                }
                else 
                { 
                    if (_activityLog.StatusInfo.Status == StatusType.Fail && txtLocalPercentage.Text.Equals("100%"))
                    {
                        txtLocalPercentage.Text = "99.98%";
                    }
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

        public string FormatMilliseconds(TimeSpan obj)
        {
            StringBuilder sb = new StringBuilder();
            if (obj.Hours != 0)
            {
                sb.Append(obj.Hours);
                sb.Append(" ");
                sb.Append(Resources["h"].ToString());
                sb.Append(" ");
            }
            if (obj.Minutes != 0 || sb.Length != 0)
            {
                sb.Append(obj.Minutes);
                sb.Append(" ");
                sb.Append(Resources["min"].ToString());
                sb.Append(" ");
            }
            if (obj.Seconds != 0 || sb.Length != 0)
            {
                sb.Append(obj.Seconds);
                sb.Append(" ");
                sb.Append(Resources["sec"].ToString());
                sb.Append(" ");
            }
            if (obj.Milliseconds != 0 || sb.Length != 0)
            {
                sb.Append(obj.Milliseconds);
                sb.Append(" ");
                sb.Append(Resources["ms"].ToString());
                sb.Append(" ");
            }
            if (sb.Length == 0)
            {
                sb.Append(0);
                sb.Append(" ");
                sb.Append(Resources["ms"].ToString());
            }
            return sb.ToString();
        }

        public void SetApplicationLanguage(string option)
        {
            ResourceDictionary dict = new ResourceDictionary();

            switch (option)
            {
                case "tr":
                    dict.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);
                    break;
                case "en":
                    dict.Source = new Uri("..\\Resources\\Lang\\string_eng.xaml", UriKind.Relative);
                    break;
                default:
                    dict.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);
                    break;
            }
            Resources.MergedDictionaries.Add(dict);
        }

    }
}
