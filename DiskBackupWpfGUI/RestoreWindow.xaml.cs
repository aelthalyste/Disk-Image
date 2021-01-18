using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler;
using DiskBackupWpfGUI.Utils;
using Serilog;
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
        //volume ise true, disk ise false
        private TaskInfo _taskInfo = new TaskInfo();

        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IRestoreTaskDal _restoreTaskDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IConfigurationDataDal _configurationDataDal;
        private ITaskSchedulerManager _schedulerManager;
        private IBackupService _backupService;
        private ILogger _logger;

        public RestoreWindow(BackupInfo backupInfo, List<VolumeInfo> volumeInfoList, IRestoreTaskDal restoreTaskDal, IStatusInfoDal statusInfoDal, ITaskInfoDal taskInfoDal, ITaskSchedulerManager schedulerManager, IBackupService backupService, ILogger logger, IConfigurationDataDal configurationDataDal)
        {
            InitializeComponent();

            _backupInfo = backupInfo;
            _volumeInfoList = volumeInfoList;
            _restoreTaskDal = restoreTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;
            _schedulerManager = schedulerManager;
            _backupService = backupService;
            _logger = logger.ForContext<RestoreWindow>();

            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            _taskInfo.RestoreTaskInfo = new RestoreTask();
            _taskInfo.BackupStorageInfo = new BackupStorageInfo();
            _taskInfo.StatusInfo = new StatusInfo();

            if (_volumeInfoList.Count == 1)
            {
                if (Convert.ToBoolean(backupInfo.OSVolume)) // bootable true ise işletim sistemi var
                    _taskInfo.RestoreTaskInfo.Type = RestoreType.RestoreDisk;
                else
                    _taskInfo.RestoreTaskInfo.Type = RestoreType.RestoreVolume;
            }
            else
            {
                _taskInfo.RestoreTaskInfo.Type = RestoreType.RestoreDisk;
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
                if (_taskInfo.RestoreTaskInfo.Type == RestoreType.RestoreVolume)
                {
                    lblDisk2Restore.Text = _volumeInfoList[0].Letter.ToString();
                }
                else
                {
                    lblDisk2Restore.Text = _volumeInfoList[0].DiskName;
                }
            }
        }

        private bool CheckAndBreakAffectedTask(TaskInfo taskInfo)
        {
            var taskList = _taskInfoDal.GetList(x => x.EnableDisable != TecnicalTaskStatusType.Broken);
            bool checkFlag = false;
            foreach (var itemTask in taskList)
            {
                foreach (var itemObje in taskInfo.StrObje) // bozulacak backup volumeleri
                {
                    if (itemTask.StrObje.Contains(itemObje))
                    {
                        checkFlag = true;
                    }
                }
            }

            MessageBoxResult result = MessageBoxResult.Yes;

            if (checkFlag)
                result = MessageBox.Show(Resources["taskAffectedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            
            if (result == MessageBoxResult.No)
                return false;

            if (result == MessageBoxResult.Yes && checkFlag)
            {
                foreach (var itemTask in taskList)
                {
                    foreach (var itemObje in taskInfo.StrObje) // bozulacak backup volumeleri
                    {
                        if (itemTask.StrObje.Contains(itemObje))
                        {
                            itemTask.EnableDisable = TecnicalTaskStatusType.Broken;

                            if (itemTask.ScheduleId != null && itemTask.ScheduleId != "")
                            {
                                _schedulerManager.DeleteJob(itemTask.ScheduleId);
                                itemTask.ScheduleId = "";
                            }

                            _taskInfoDal.Update(itemTask);
                        }
                    }
                }
            }
            return true;
        }

        private void btnRestoreOk_Click(object sender, RoutedEventArgs e)
        {
            //Kaydedip Silinecek
            if (dtpSetTime.Value <= DateTime.Now + TimeSpan.FromSeconds(10))
            {
                MessageBox.Show(Resources["notSchedulePastDaysMB"].ToString(), Resources["MessageboxTitle"].ToString(),  MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else
            {
                bool nullControlFlag = true;
                if (rbStartNow.IsChecked.Value) // hemen çalıştır
                {
                    if (txtTaskName.Text.Equals(""))
                    {
                        MessageBox.Show(Resources["notNullMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                        nullControlFlag = false;
                    }
                }
                else if (rbSetTime.IsChecked.Value) // zaman belirle
                {
                    if (txtTaskName.Text.Equals("") || dtpSetTime.Value.Equals(""))
                    {
                        MessageBox.Show(Resources["notNullMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
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
                        _taskInfo.NextDate = Convert.ToDateTime("01/01/0002");
                    else
                        _taskInfo.NextDate = (DateTime)dtpSetTime.Value;
                    _taskInfo.RestoreTaskInfo.BackupVersion = _backupInfo.Version;
                    _taskInfo.RestoreTaskInfo.RootDir = _backupInfo.BackupStorageInfo.Path + _backupInfo.FileName;
                    _taskInfo.RestoreTaskInfo.SourceLetter = _backupInfo.Letter.ToString();

                    _taskInfo.BackupStorageInfo = _backupInfo.BackupStorageInfo;  // gelen backup'ın storageInfosu mevcut -- DEĞİŞECEK
                    _taskInfo.BackupStorageInfoId = _backupInfo.BackupStorageInfoId;
                    _taskInfo.Obje = _volumeInfoList.Count; // kaç tane volumede değişiklik olacağı
                    // ortak alan bitti

                    if (_taskInfo.RestoreTaskInfo.Type == RestoreType.RestoreVolume) // volume
                    {
                        //görevlerde kontrol için gerekli
                        _taskInfo.StrObje = _volumeInfoList[0].Letter.ToString();
                        _taskInfo.RestoreTaskInfo.TargetLetter = _volumeInfoList[0].Letter.ToString();

                        TaskInfo resultTaskInfo = SaveToDatabase();

                        if (resultTaskInfo != null)
                        {
                            MessageBox.Show(Resources["addSuccessMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                            if (_taskInfo.NextDate == Convert.ToDateTime("01/01/0002")) // hemen çalıştır
                            {
                                _taskInfo.LastWorkingDate = DateTime.Now;
                                _taskInfoDal.Update(resultTaskInfo);
                                _schedulerManager.RestoreVolumeNowJob(resultTaskInfo).Wait();
                            }
                            else
                                _schedulerManager.RestoreVolumeJob(resultTaskInfo).Wait();
                        }
                        
                    }
                    else // disk
                    {
                        //görevlerde kontrol için gerekli
                        foreach (var item in _volumeInfoList)
                        {
                            _taskInfo.StrObje += item.Letter.ToString();
                        }

                        _taskInfo.RestoreTaskInfo.DiskId = Convert.ToInt32(_volumeInfoList[0].DiskName.Split(' ')[1]);

                        try
                        {
                            _taskInfo.RestoreTaskInfo.TargetLetter = _backupService.AvailableVolumeLetter().ToString();
                            Console.WriteLine("NarDIWrapper'dan alınan harf: " + _taskInfo.RestoreTaskInfo.TargetLetter);
                        }
                        catch(Exception ex)
                        {
                            _logger.Error(ex, "NarDIWrapper'dan uygun disk için harf alınamadı."); 
                            MessageBox.Show(Resources["notAvailableVolumeLetterMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                            Close();
                            return;
                        }

                        var result = CheckAndBreakAffectedTask(_taskInfo);

                        if (result)
                        {
                            TaskInfo resultTaskInfo = SaveToDatabase();

                            if (resultTaskInfo != null)
                            {
                                MessageBox.Show(Resources["addSuccessMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                                if (_taskInfo.NextDate == Convert.ToDateTime("01/01/0002")) // hemen çalıştır
                                {
                                    _taskInfo.LastWorkingDate = DateTime.Now;
                                    _taskInfoDal.Update(resultTaskInfo);
                                    _schedulerManager.RestoreDiskNowJob(resultTaskInfo).Wait();
                                }
                                else
                                    _schedulerManager.RestoreDiskJob(resultTaskInfo).Wait();
                            }
                        }
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
                MessageBox.Show(Resources["addFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            //backupTask kaydetme
            _taskInfo.StatusInfo.TaskName = _taskInfo.Name;
            _taskInfo.StatusInfo.SourceObje = _taskInfo.StrObje;
            var resultStatusInfo = _statusInfoDal.Add(_taskInfo.StatusInfo);
            if (resultStatusInfo == null)
            {
                MessageBox.Show(Resources["addFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            // task kayıdı
            _taskInfo.Status = TaskStatusType.FirstMissionExpected;
            _taskInfo.StatusInfoId = resultStatusInfo.Id;
            _taskInfo.RestoreTaskId = resultRestoreTask.Id;
            _taskInfo.LastWorkingDate = Convert.ToDateTime("01/01/0002");
            var resultTaskInfo = _taskInfoDal.Add(_taskInfo);
            if (resultTaskInfo == null)
            {
                MessageBox.Show(Resources["addFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
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
