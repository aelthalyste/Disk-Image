using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler;
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
    /// Interaction logic for Restore.xaml
    /// </summary>
    public partial class RestoreWindow : Window
    {
        private BackupInfo _backupInfo;
        private List<VolumeInfo> _volumeInfoList = new List<VolumeInfo>();
        private bool _volumeOrFreshDisk = false; //volume ise true, disk ise false
        private TaskInfo _taskInfo = new TaskInfo();

        private ITaskInfoDal _taskInfoDal;
        private IRestoreTaskDal _restoreTaskDal;
        private IStatusInfoDal _statusInfoDal;
        private ITaskSchedulerManager _schedulerManager;

        public RestoreWindow(BackupInfo backupInfo, List<VolumeInfo> volumeInfoList, IRestoreTaskDal restoreTaskDal, IStatusInfoDal statusInfoDal, ITaskInfoDal taskInfoDal, ITaskSchedulerManager schedulerManager)
        {
            InitializeComponent();

            _backupInfo = backupInfo;
            _volumeInfoList = volumeInfoList;
            _restoreTaskDal = restoreTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;
            _schedulerManager = schedulerManager;
            _taskInfo.RestoreTaskInfo = new RestoreTask();
            _taskInfo.BackupStorageInfo = new BackupStorageInfo();
            _taskInfo.StatusInfo = new StatusInfo();

            if (_volumeInfoList.Count == 1)
            {
                if (volumeInfoList[0].Bootable) // bootable true ise işletim sistemi var
                    _volumeOrFreshDisk = false;
                else
                    _volumeOrFreshDisk = true;
            }
            else
            {
                _volumeOrFreshDisk = false;
            }

            //foreach (var item in _volumeInfoList)
            //{
            //    MessageBox.Show(item.DiskName + " " + item.Letter + " " + item.Name);
            //}
            dtpSetTime.Value = DateTime.Now + TimeSpan.FromMinutes(5);
            //MessageBox.Show(backupInfo.BackupStorageInfo.StorageName + " " + backupInfo.BackupStorageInfoId);
        }

        #region Title Bar
        private void btnRestoreClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void btnRestoreMin_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }
        #endregion

        #region Next-Back-Ok-Cancel Button
        private void btnRestoreBack_Click(object sender, RoutedEventArgs e)
        {
            if (RTabControl.SelectedIndex != 0)
            {
                RTabControl.SelectedIndex -= 1;
            }
        }

        private void btnRestoreNext_Click(object sender, RoutedEventArgs e)
        {
            if (RTabControl.SelectedIndex != 3)
            {
                RTabControl.SelectedIndex += 1;
            }

            // özet yazımı
            if (RTabControl.SelectedIndex == 2)
            {
                lblTaskName.Text = txtTaskName.Text;

                if (rbStartNow.IsChecked.Value) // hemen çalıştır
                {
                    lblSchedulerTasks.Text = Resources["startNow"].ToString();
                }
                else if (rbSetTime.IsChecked.Value) // şu saatte çalıştır
                {
                    lblSchedulerTasks.Text = dtpSetTime.Text;
                }
                lblArea2Restore.Text = _backupInfo.Letter.ToString();
                if (_volumeOrFreshDisk)
                {
                    lblDisk2Restore.Text = _volumeInfoList[0].Letter.ToString();
                }
                else
                {
                    lblDisk2Restore.Text = _volumeInfoList[0].DiskName;
                }
            }
        }

        private void btnRestoreOk_Click(object sender, RoutedEventArgs e)
        {
            //Kaydedip Silinecek

            if (dtpSetTime.Value <= DateTime.Now + TimeSpan.FromSeconds(10))
            {
                MessageBox.Show("Geçmiş tarih için geri yükleme işlemi gerçekleştirilemez.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else
            {
                bool nullControlFlag = true;
                if (rbStartNow.IsChecked.Value) // hemen çalıştır
                {
                    if (txtTaskName.Text.Equals("") || txtTaskDescription.Text.Equals(""))
                    {
                        MessageBox.Show("İlgili alanları lütfen boş geçmeyiniz. Hemen çalıştır", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                        nullControlFlag = false;
                    }
                }
                else if (rbSetTime.IsChecked.Value) // zaman belirle
                {
                    if (txtTaskName.Text.Equals("") || txtTaskDescription.Text.Equals("") || dtpSetTime.Value.Equals(""))
                    {
                        MessageBox.Show("İlgili alanları lütfen boş geçmeyiniz. Zaman belirle", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                        nullControlFlag = false;
                    }
                }
                if (nullControlFlag) // kayıt et
                {
                    // ortak alan başladı
                    _taskInfo.Type = TaskType.Restore;
                    _taskInfo.Name = txtTaskName.Text;
                    _taskInfo.Descripiton = txtTaskDescription.Text;
                    if (rbStartNow.IsChecked.Value)
                        _taskInfo.NextDate = DateTime.Now + TimeSpan.FromSeconds(10);
                    else
                        _taskInfo.NextDate = (DateTime)dtpSetTime.Value;
                    _taskInfo.RestoreTaskInfo.BackupVersion = _backupInfo.Version;
                    _taskInfo.BackupStorageInfo = _backupInfo.BackupStorageInfo;  // gelen backup'ın storageInfosu mevcut
                    _taskInfo.BackupStorageInfoId = _backupInfo.BackupStorageInfoId;
                    _taskInfo.Obje = _volumeInfoList.Count;
                    // ortak alan bitti

                    if (_volumeOrFreshDisk) // volume
                    {
                        _taskInfo.StrObje = _volumeInfoList[0].Letter.ToString();
                        _taskInfo.RestoreTaskInfo.DiskLetter = _volumeInfoList[0].Letter.ToString();

                        TaskInfo resultTaskInfo = SaveToDatabase();

                        if (resultTaskInfo != null)
                            MessageBox.Show("Ekleme işlemi başarılı");

                        _schedulerManager.RestoreVolumeJob(resultTaskInfo).Wait();
                    }
                    else // disk
                    {
                        foreach (var item in _volumeInfoList)
                        {
                            _taskInfo.StrObje += item.Letter.ToString();
                        }
                        var result = _volumeInfoList[0].DiskName.Split(' ');
                        _taskInfo.RestoreTaskInfo.DiskId = Convert.ToInt32(result[1]);

                        TaskInfo resultTaskInfo = SaveToDatabase();

                        if (resultTaskInfo != null)
                            MessageBox.Show("Ekleme işlemi başarılı");

                        //scheduler
                    }
                    Close();
                }
            }
        }

        private TaskInfo SaveToDatabase()
        {
            //backupTask kaydetme
            var resultRestoreTask = _restoreTaskDal.Add(_taskInfo.RestoreTaskInfo);
            if (resultRestoreTask == null)
            {
                MessageBox.Show("Ekleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            //backupTask kaydetme
            _taskInfo.StatusInfo.TaskName = _taskInfo.Name;
            _taskInfo.StatusInfo.SourceObje = _taskInfo.StrObje;
            var resultStatusInfo = _statusInfoDal.Add(_taskInfo.StatusInfo);
            if (resultStatusInfo == null)
            {
                MessageBox.Show("Ekleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            // task kayıdı
            _taskInfo.Status = Resources["FirstMissionExpected"].ToString();
            _taskInfo.StatusInfoId = resultStatusInfo.Id;
            _taskInfo.RestoreTaskId = resultRestoreTask.Id;
            _taskInfo.LastWorkingDate = Convert.ToDateTime("01/01/0002");
            var resultTaskInfo = _taskInfoDal.Add(_taskInfo);
            if (resultTaskInfo == null)
            {
                MessageBox.Show("Ekleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            return resultTaskInfo;
        }

        private void btnRestoreCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        #endregion

        private void RTabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (RTabControl.SelectedIndex == 0)
            {
                lblTabHeader.Text = Resources["name"].ToString();
                lblTabContent.Text = Resources["RNameContent"].ToString();
                btnRestoreBack.IsEnabled = false;
            }
            else if (RTabControl.SelectedIndex == 1)
            {
                lblTabHeader.Text = Resources["scheduler"].ToString();
                lblTabContent.Text = Resources["RSchedulerContent"].ToString();
                btnRestoreBack.IsEnabled = true;
            }
            else if (RTabControl.SelectedIndex == 2)
            {
                lblTabHeader.Text = Resources["advancedOptions"].ToString();
                lblTabContent.Text = Resources["restoreDiskContent1"].ToString();
                btnRestoreNext.IsEnabled = true;
            }
            else if (RTabControl.SelectedIndex == 3)
            {
                lblTabHeader.Text = Resources["summary"].ToString();
                lblTabContent.Text = Resources["RSummaryContent"].ToString();
                btnRestoreNext.IsEnabled = false;
            }
        }

        private void rbSetTime_Checked(object sender, RoutedEventArgs e)
        {
            stackSetTime.IsEnabled = true;
        }

        private void rbSetTime_Unchecked(object sender, RoutedEventArgs e)
        {
            stackSetTime.IsEnabled = false;
        }

        private void btnAdvancedOptions_Click(object sender, RoutedEventArgs e)
        {

        }

        private void checkBootPartition_Checked(object sender, RoutedEventArgs e)
        {
            stackBootCheck.IsEnabled = true;
        }

        private void checkBootPartition_Unchecked(object sender, RoutedEventArgs e)
        {
            stackBootCheck.IsEnabled = false;
            rbBootGPT.IsChecked = true;
            rbBootGPT.IsChecked = false;
        }
    }
}
