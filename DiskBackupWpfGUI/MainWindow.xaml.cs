﻿using Autofac;
using Autofac.Core.Lifetime;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler;
using DiskBackup.TaskScheduler.Jobs;
using Serilog;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
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
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private int _diskExpenderIndex = 0;

        private bool _diskAllControl = false;
        private bool _diskAllHeaderControl = false;

        private bool _storegeAllControl = false;
        private bool _viewBackupsAllControl = false;
        private bool _tasksAllControl = false;

        private List<CheckBox> _expanderCheckBoxes = new List<CheckBox>();
        private List<int> _numberOfItems = new List<int>();
        private List<string> _groupName = new List<string>();
        private List<CheckBox> _restoreExpanderCheckBoxes = new List<CheckBox>();
        private List<int> _restoreNumberOfItems = new List<int>();
        private List<string> _restoreGroupName = new List<string>();
        private List<Expander> _expanderRestoreDiskList = new List<Expander>();
        private List<DiskInformation> _diskList = new List<DiskInformation>();
        private List<VolumeInfo> _volumeList = new List<VolumeInfo>();
        private List<TaskInfo> _taskInfoList = new List<TaskInfo>();
        private List<ActivityLog> _activityLogList = new List<ActivityLog>();
        private List<BackupInfo> _backupsItems = new List<BackupInfo>();
        private List<ActivityDownLog> _logList = new List<ActivityDownLog>();

        private CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();

        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IBackupTaskDal _backupTaskDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;
        private readonly IRestoreTaskDal _restoreTaskDal;
        //private IBackupService _backupService;

        private readonly ILifetimeScope _scope;
        private readonly ILogger _logger;

        public MainWindow(ILifetimeScope scope, ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IBackupTaskDal backupTaskDal, IStatusInfoDal statusInfoDal, IActivityLogDal activityLogDal, ILogger logger, IRestoreTaskDal restoreTaskDal)
        {
            InitializeComponent();

            _logger = logger.ForContext<MainWindow>();

            _backupStorageDal = backupStorageDal;
            _activityLogDal = activityLogDal;
            _backupTaskDal = backupTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;
            _restoreTaskDal = restoreTaskDal;

            _scope = scope;
            var backupService = _scope.Resolve<IBackupService>();
            var backupStorageService = _scope.Resolve<IBackupStorageService>();
            if (!backupService.GetInitTracker())
                MessageBox.Show("Driver intialize edilemedi!", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);


            #region Disk Bilgileri

            try
            {
                _logger.Verbose("GetDiskList metoduna istekte bulunuldu");
                _diskList = backupService.GetDiskList();

                foreach (var diskItem in _diskList)
                {
                    foreach (var volumeItem in diskItem.VolumeInfos)
                    {
                        _volumeList.Add(volumeItem);
                    }
                }

                listViewDisk.ItemsSource = _volumeList;
                listViewRestoreDisk.ItemsSource = _volumeList;

                CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewDisk.ItemsSource);
                PropertyGroupDescription groupDescription = new PropertyGroupDescription("DiskName");
                view.GroupDescriptions.Add(groupDescription);

                GetDiskPage();
            }
            catch (Exception e)
            {
                _logger.Error(e, "Disk bilgileri getirilemedi!");
            }

            #endregion


            #region Yedekleme alanları

            _logger.Verbose("GetDiskList metoduna istekte bulunuldu");
            _logger.Verbose("GetBackupStorages istekte bulunuldu");

            listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, backupStorageService.BackupStorageInfoList());

            #endregion


            #region Görevler

            GetTasks();

            #endregion


            #region ActivityLog
            ShowActivityLog();
            _logger.Verbose("GetDownLogList metoduna istekte bulunuldu");
            _logList = backupService.GetDownLogList();
            listViewLogDown.ItemsSource = _logList;

            #endregion


            #region Backup dosya bilgileri

            try
            {
                _logger.Verbose("GetBackupFileList metoduna istekte bulunuldu");
                _backupsItems = backupService.GetBackupFileList(_backupStorageDal.GetList());

                listViewBackups.ItemsSource = _backupsItems;
                listViewRestore.ItemsSource = _backupsItems;
            }
            catch (Exception e)
            {
                _logger.Error(e, "Backup dosyaları getirilemedi!");
            }

            #endregion

            RefreshTasks(_cancellationTokenSource.Token, backupService);
            this.Closing += (sender, e) => _cancellationTokenSource.Cancel();
        }

        private void GetDiskPage()
        {
            _logger.Verbose("GetDiskPage istekte bulunuldu, Disk Pro.Bar'ları güncelleniyor");
            diskInfoStackPanel.Children.Clear();
            stackTasksDiskInfo.Children.Clear();

            foreach (var item in _diskList)
            {
                //denemek için burayı kaldır
                /*foreach (var item2 in item.VolumeInfos)
                {
                    item2.Size += 100000000000;
                }*/
                DiskInfoPage page = new DiskInfoPage(item);
                Frame frame = new Frame();
                frame.Content = page;
                frame.VerticalAlignment = VerticalAlignment.Top;
                diskInfoStackPanel.Children.Add(frame);
                page = new DiskInfoPage(item);
                frame = new Frame();
                frame.Content = page;
                frame.VerticalAlignment = VerticalAlignment.Top;
                stackTasksDiskInfo.Children.Add(frame);
            }
        }


        #region Title Bar
        private void btnMainClose_Click(object sender, RoutedEventArgs e)
        {
            Environment.Exit(0);
        }

        private void btnMainMin_Click(object sender, RoutedEventArgs e)
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


        #region Tasks Create Tab

        private void btnCreateTask_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnCreateTask_Click istekte bulunuldu");

            List<BackupStorageInfo> backupStorageInfoList = new List<BackupStorageInfo>();
            foreach (BackupStorageInfo item in listViewBackupStorage.Items)
            {
                backupStorageInfoList.Add(item);
            }

            List<VolumeInfo> volumeInfoList = new List<VolumeInfo>();
            foreach (VolumeInfo item in listViewDisk.SelectedItems)
            {
                volumeInfoList.Add(item);
            }

            using (var scope = _scope.BeginLifetimeScope())
            {
                NewCreateTaskWindow newCreateTask = scope.Resolve<NewCreateTaskWindow>(new TypedParameter(backupStorageInfoList.GetType(), backupStorageInfoList),
                    new TypedParameter(volumeInfoList.GetType(), volumeInfoList));
                newCreateTask.ShowDialog();
            }
            GetTasks();
            var _backupStorageService = _scope.Resolve<IBackupStorageService>();
            _logger.Verbose("GetBackupStorages istekte bulunuldu");
            listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, _backupStorageService.BackupStorageInfoList());
        }

        private void Expander_Loaded(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("Expander_Loaded istekte bulunuldu");

            var expander = sender as Expander;

            var size = expander.FindName("txtTotalSize") as TextBlock;
            var expanderCheck = expander.FindName("HeaderCheckBox") as CheckBox;
            _expanderCheckBoxes.Add(expanderCheck);
            var numberOfItems = expander.FindName("txtNumberOfItems") as TextBlock;
            _numberOfItems.Add(Convert.ToInt32(numberOfItems.Text));
            var groupName = expander.FindName("txtGroupName") as TextBlock;
            _groupName.Add(groupName.Text);
            if (_diskList.Count == _diskExpenderIndex)
                _diskExpenderIndex = 0;
            size.Text = FormatBytes(_diskList[_diskExpenderIndex++].Size);
        }
        private void listViewDisk_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewDisk.SelectedIndex != -1)
            {
                btnCreateTask.IsEnabled = true;
            }
            else
            {
                btnCreateTask.IsEnabled = false;
            }
        }

        #region Checkbox Operations
        private void chbDiskSelectDiskAll_Checked(object sender, RoutedEventArgs e)
        {
            if (!_diskAllControl)
            {
                foreach (VolumeInfo item in listViewDisk.ItemsSource)
                {
                    listViewDisk.SelectedItems.Add(item);
                }
                _diskAllControl = true;
                _expanderCheckBoxes.ForEach(cb => cb.IsChecked = true);
            }
        }

        private void chbDiskSelectDiskAll_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_diskAllControl)
            {
                _expanderCheckBoxes.ForEach(cb => cb.IsChecked = false);
                listViewDisk.SelectedItems.Clear();
                _diskAllControl = false;
            }
        }

        private void chbDisk_Checked(object sender, RoutedEventArgs e)
        {
            if (chbDiskSelectDiskAll.IsChecked == false)
            {
                _diskAllControl = listViewDisk.SelectedItems.Count == listViewDisk.Items.Count;
                chbDiskSelectDiskAll.IsChecked = _diskAllControl;
            }

            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as VolumeInfo; //data seçilen değer

            for (int i = 0; i < _groupName.Count; i++)
            {
                if (data.DiskName.Equals(_groupName[i]))
                {
                    int totalSelected = 0;
                    //i kaçıncı sıradaki adete eşit olacağı
                    foreach (VolumeInfo item in listViewDisk.SelectedItems)
                    {
                        if (item.DiskName.Equals(data.DiskName))
                        {
                            totalSelected++;
                        }
                    }
                    if (_numberOfItems[i] == totalSelected)
                    {
                        _expanderCheckBoxes[i].IsChecked = true;
                    }
                }
            }
        }

        private void chbDisk_Unchecked(object sender, RoutedEventArgs e)
        {
            _diskAllControl = false;
            chbDiskSelectDiskAll.IsChecked = false;
            _diskAllHeaderControl = false;

            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as VolumeInfo; //data seçilen değer

            for (int i = 0; i < _groupName.Count; i++)
            {
                if (_groupName[i].Equals(data.DiskName))
                {
                    _expanderCheckBoxes[i].IsChecked = false;
                    _diskAllHeaderControl = true;
                }
            }
        }

        private void HeaderCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            var headerCheckBox = sender as CheckBox;
            _diskAllHeaderControl = true;

            foreach (VolumeInfo item in listViewDisk.Items)
            {
                if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewDisk.SelectedItems.Add(item);
                }
            }
        }

        private void HeaderCheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_diskAllHeaderControl)
            {
                var headerCheckBox = sender as CheckBox;
                foreach (VolumeInfo item in listViewDisk.Items)
                {
                    if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                    {
                        listViewDisk.SelectedItems.Remove(item);
                    }
                }
                _diskAllHeaderControl = true;
            }
        }

        #endregion


        #endregion


        #region Tasks Tab

        private void btnTaskDelete_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnTaskDelete_Click istekte bulunuldu");

            MessageBoxResult result = MessageBox.Show($"{listViewTasks.SelectedItems.Count} adet veri silinecek. Onaylıyor musunuz?", "Narbulut diyor ki; ", MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            if (result == MessageBoxResult.Yes)
            {
                foreach (TaskInfo item in listViewTasks.SelectedItems)
                {
                    // ilgili triggerı sil
                    if (item.ScheduleId != null && !item.ScheduleId.Contains("Now") && item.ScheduleId != "")
                    {
                        var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                        taskSchedulerManager.DeleteJob(item.ScheduleId);
                    }

                    // task tipine göre ilgili task tablosundan bilgisini sil
                    if (item.Type == TaskType.Backup)
                    {
                        _backupTaskDal.Delete(_backupTaskDal.Get(x => x.Id == item.BackupTaskId));
                    }
                    else
                    {
                        //restore silme
                        _restoreTaskDal.Delete(_restoreTaskDal.Get(x => x.Id == item.RestoreTaskId));
                    }

                    // ilgili status infosunu sil
                    _statusInfoDal.Delete(_statusInfoDal.Get(x => x.Id == item.StatusInfoId));

                    // kendisini sil
                    _taskInfoDal.Delete(item);
                }
                GetTasks();
            }
        }

        private void listViewTasks_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewTasks.SelectedIndex != -1)
            {
                btnTaskDelete.IsEnabled = true;
                btnTaskEdit.IsEnabled = true;

                //butonlar eklenmeye devam edecek burayada da checkboxlara da

                // çalışan görevi start etme engellendi
                ToggleTaskButtons();
            }
        }

        private void listViewTasks_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            _logger.Verbose("listViewTasks_MouseDoubleClick istekte bulunuldu");

            if (listViewTasks.SelectedIndex != -1)
            {
                TaskInfo taskInfo = (TaskInfo)listViewTasks.SelectedItem;
                taskInfo.StatusInfo = _statusInfoDal.Get(x => x.Id == taskInfo.StatusInfoId);
                taskInfo.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == taskInfo.BackupTaskId);
                StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("statusInfo", taskInfo.StatusInfo));
                backupStatus.Show();
            }
        }

        private void btnTaskStart_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnTaskStart_Click istekte bulunuldu");

            TaskInfo taskInfo = (TaskInfo)listViewTasks.SelectedItem;
            var backupService = _scope.Resolve<IBackupService>();

            if (taskInfo.Status.Equals(TaskStatusType.Ready) || taskInfo.Status.Equals(TaskStatusType.FirstMissionExpected))
            {
                taskInfo.LastWorkingDate = DateTime.Now;
                taskInfo.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == taskInfo.BackupStorageInfoId);
                taskInfo.StatusInfo = _statusInfoDal.Get(x => x.Id == taskInfo.StatusInfoId);
                Console.WriteLine("Hemen çalıştırılıyor");
                if (taskInfo.Type == TaskType.Backup)
                {
                    Console.WriteLine("Backup başlatılıyor");
                    taskInfo.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == taskInfo.BackupTaskId);
                    if (taskInfo.BackupTaskInfo.Type == BackupTypes.Diff || taskInfo.BackupTaskInfo.Type == BackupTypes.Inc)
                    {
                        Console.WriteLine("Backup Inc-Diff başlatılıyor");
                        var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                        if (taskInfo.ScheduleId != null && !taskInfo.ScheduleId.Contains("Now") && taskInfo.ScheduleId != "")
                        {
                            taskSchedulerManager.RunNowTrigger(taskInfo.ScheduleId).Wait();
                        }
                        else
                        {
                            taskSchedulerManager.BackupIncDiffNowJob(taskInfo).Wait();
                        }
                        StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("statusInfo", taskInfo.StatusInfo));
                        backupStatus.Show();
                    }
                    else
                    {
                        //full
                    }
                }
                else
                {
                    //restore
                    Console.WriteLine("Restore başlatılıyor");
                    taskInfo.RestoreTaskInfo = _restoreTaskDal.Get(x => x.Id == taskInfo.RestoreTaskId);
                    if (taskInfo.RestoreTaskInfo.Type == RestoreType.RestoreVolume)
                    {
                        // restore disk ve volume ayrıntısı burada kontrol edilip çağırılacak
                        var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                        taskSchedulerManager.RestoreVolumeNowJob(taskInfo).Wait();
                    }
                    else
                    {
                        // restore disk ve volume ayrıntısı burada kontrol edilip çağırılacak
                        var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                        taskSchedulerManager.RestoreDiskNowJob(taskInfo).Wait();
                    }

                }
            }
            else if (taskInfo.Status.Equals(TaskStatusType.Paused))
            {
                backupService.ResumeTask(taskInfo);
            }
            RefreshBackupsandTasks(backupService);
        }

        private void btnTaskPause_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnTaskPause_Click istekte bulunuldu");

            var backupService = _scope.Resolve<IBackupService>();
            backupService.PauseTask((TaskInfo)listViewTasks.SelectedItem);
            RefreshBackupsandTasks(backupService);
        }

        private void btnTaskStop_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnTaskStop_Click istekte bulunuldu");

            var backupService = _scope.Resolve<IBackupService>();
            backupService.CancelTask((TaskInfo)listViewTasks.SelectedItem);
            RefreshBackupsandTasks(backupService);
        }

        private void btnEnableTask_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnEnableTask_Click istekte bulunuldu");

            var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
            foreach (TaskInfo item in listViewTasks.SelectedItems)
            {
                taskSchedulerManager.EnableSchedule(item).Wait();
            }
            GetTasks();
        }

        private void btnDisableTask_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnDisableTask_Click istekte bulunuldu");

            var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
            foreach (TaskInfo item in listViewTasks.SelectedItems)
            {
                taskSchedulerManager.DisableSchedule(item).Wait();
            }
            GetTasks();
        }

        private void btnCopyTask_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnCopyTask_Click istekte bulunuldu");

            var task = (TaskInfo)listViewTasks.SelectedItem;
            task.Name = "(" + Resources["clone"] + ") " + task.Name;
            task.EnableDisable = 1;
            task.LastWorkingDate = Convert.ToDateTime("01/01/0002");

            var backupTask = _backupTaskDal.Get(x => x.Id == task.BackupTaskId);
            backupTask.TaskName = task.Name;
            var resultBackupTask = _backupTaskDal.Add(backupTask);
            if (resultBackupTask == null)
                MessageBox.Show("Kopyalam işlemi başarısız.", "Narbulut diyor ki;", MessageBoxButton.OK, MessageBoxImage.Error);
            else
            {
                var statusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
                statusInfo.TaskName = task.Name;
                var resultStatusInfo = _statusInfoDal.Add(statusInfo);
                if (resultStatusInfo == null)
                    MessageBox.Show("Kopyalam işlemi başarısız.", "Narbulut diyor ki;", MessageBoxButton.OK, MessageBoxImage.Error);
                else
                {
                    string lastSchedulerId = task.ScheduleId;
                    task.ScheduleId = "";
                    task.BackupTaskId = resultBackupTask.Id;
                    task.BackupTaskInfo = resultBackupTask;
                    task.StatusInfoId = resultStatusInfo.Id;
                    task.StatusInfo = resultStatusInfo;
                    TaskInfo resultTask = _taskInfoDal.Add(task);
                    if (resultTask == null)
                        MessageBox.Show("Kopyalam işlemi başarısız.", "Narbulut diyor ki;", MessageBoxButton.OK, MessageBoxImage.Error);
                    else
                    {
                        // scheduler oluştur
                        Console.WriteLine("resultTask Id:" + resultTask.Id);
                        CreateBackupTaskScheduler(resultTask, lastSchedulerId);
                    }
                }
            }
            GetTasks();
        }

        private void CreateBackupTaskScheduler(TaskInfo resultTask, string lastSchedulerId)
        {
            _logger.Verbose("CreateBackupTaskScheduler istekte bulunuldu");

            var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();

            if (resultTask.BackupTaskInfo.Type == BackupTypes.Diff || resultTask.BackupTaskInfo.Type == BackupTypes.Inc)
            {
                if (resultTask.BackupTaskInfo.AutoRun)
                {
                    if (resultTask.BackupTaskInfo.AutoType == AutoRunType.DaysTime)
                    {
                        if (lastSchedulerId.Contains("Everyday")) // everyday
                        {
                            taskSchedulerManager.BackupIncDiffEverydayJob(resultTask).Wait();
                        }
                        else if (lastSchedulerId.Contains("WeekDays")) //weekdays
                        {
                            taskSchedulerManager.BackupIncDiffWeekDaysJob(resultTask).Wait();
                        }
                        else //certain
                        {
                            taskSchedulerManager.BackupIncDiffCertainDaysJob(resultTask).Wait();
                        }
                    }
                    else if (resultTask.BackupTaskInfo.AutoType == AutoRunType.WeeklyTime)
                    {
                        taskSchedulerManager.BackupIncDiffWeeklyJob(resultTask).Wait();
                    }
                    else if (resultTask.BackupTaskInfo.AutoType == AutoRunType.Periodic)
                    {
                        if (lastSchedulerId.Contains("PeriodicHours")) //saat
                        {
                            taskSchedulerManager.BackupIncDiffPeriodicHoursJob(resultTask).Wait();
                        }
                        else //dakika
                        {
                            taskSchedulerManager.BackupIncDiffPeriodicMinutesJob(resultTask).Wait();
                        }
                    }
                }
            }
            else
            {
                //full gelince buraya alıcaz paşayı
            }

            var returnTask = _taskInfoDal.Get(x => x.Id == resultTask.Id);

            // Oluşturulan scheduler'ı pasif et
            if (returnTask.ScheduleId != "" && returnTask.ScheduleId != null)
            {
                taskSchedulerManager.DisableSchedule(returnTask).Wait();
            }
            else
            {
                returnTask.EnableDisable = 0;
                _taskInfoDal.Update(returnTask);
            }
        }

        private void GetTasks()
        {
            _logger.Verbose("GetTasks metoduna istekte bulunuldu");
            _taskInfoList = _taskInfoDal.GetList();

            foreach (var item in _taskInfoList)
            {
                item.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == item.BackupStorageInfoId);
                item.StrStatus = item.Status.ToString();
                item.StrStatus = Resources[item.StrStatus].ToString();
                Console.WriteLine(item.StrStatus);
            }
            listViewTasks.ItemsSource = _taskInfoList;
            DisableTaskButtons();
        }


        #region Task Buttons


        private void ToggleTaskButtons()
        {
            _logger.Verbose("ToggleTaskButtons istekte bulunuldu");

            bool runningFlag = false;
            bool pauseFlag = false;
            bool disableFlag = false;
            bool enableFlag = false;
            bool restoreTaskFlag = false;
            foreach (TaskInfo item in listViewTasks.SelectedItems)
            {
                if (item.Status.Equals(TaskStatusType.Working))
                {
                    runningFlag = true;
                }
                else if (item.Status.Equals(TaskStatusType.Paused))
                {
                    pauseFlag = true;
                }
                else if (item.Status.Equals(TaskStatusType.Ready) || item.Status.Equals(TaskStatusType.FirstMissionExpected))
                {
                    btnTaskPause.IsEnabled = false;
                    btnTaskStop.IsEnabled = false;
                    btnTaskStart.IsEnabled = true;
                    if (item.EnableDisable == 0)
                        enableFlag = true;
                    else
                        disableFlag = true;
                }

                if (item.ScheduleId == null || item.ScheduleId == "") // schedulerId boş ise o görev için bir job oluşturulmamış demektir. Disable/Enable oluşamaz
                {
                    enableFlag = true;
                    disableFlag = true;
                }

                if (item.Type.Equals(TaskType.Restore)) // restore'da düzenleme olmadığı için restore task klonlanamaz
                    restoreTaskFlag = true;
            }

            if (runningFlag && pauseFlag)
            {
                DisableTaskButtons();
            }
            else if (runningFlag)
            {
                RunningTaskButtons();
            }
            else if (pauseFlag)
            {
                PausedTaskButtons();
            }

            if (enableFlag && disableFlag)
            {
                btnDisableTask.IsEnabled = false;
                btnEnableTask.IsEnabled = false;
            }
            else if (enableFlag)
            {
                btnDisableTask.IsEnabled = true;
                btnTaskStart.IsEnabled = true;
            }
            else
            {
                btnEnableTask.IsEnabled = true;
                btnTaskStart.IsEnabled = false;
            }

            if (restoreTaskFlag || listViewTasks.SelectedItems.Count > 1) // restore task kopyalanamaz ve editlenemez!
            {
                btnCopyTask.IsEnabled = false;
                btnTaskEdit.IsEnabled = false;
            }
            else
                btnCopyTask.IsEnabled = true;

            if ((runningFlag && restoreTaskFlag) || (pauseFlag && restoreTaskFlag)) // restore task cancel veya pause edilemez!
            {
                btnTaskPause.IsEnabled = false;
                btnTaskStop.IsEnabled = false;
            }
        }

        private void DisableTaskButtons()
        {
            _logger.Verbose("DisableTaskButtons istekte bulunuldu");

            btnDisableTask.IsEnabled = false;
            btnCopyTask.IsEnabled = false;
            btnTaskDelete.IsEnabled = false;
            btnTaskEdit.IsEnabled = false;
            btnEnableTask.IsEnabled = false;
            btnTaskPause.IsEnabled = false;
            btnTaskStart.IsEnabled = false;
            btnTaskStop.IsEnabled = false;
        }

        private void PausedTaskButtons()
        {
            _logger.Verbose("PausedTaskButtons istekte bulunuldu");

            btnTaskStart.IsEnabled = true;
            btnTaskEdit.IsEnabled = false;
            btnTaskPause.IsEnabled = false;
            btnTaskStop.IsEnabled = true;
            //diğer butonlar gelecek
            btnTaskDelete.IsEnabled = false;
            btnDisableTask.IsEnabled = false;
            btnEnableTask.IsEnabled = false;
        }

        private void RunningTaskButtons()
        {
            _logger.Verbose("RunningTaskButtons istekte bulunuldu");

            btnTaskStart.IsEnabled = false;
            btnTaskEdit.IsEnabled = false; // çalışan görev düzenlenemez
            btnTaskPause.IsEnabled = true;
            btnTaskStop.IsEnabled = true;
            btnTaskDelete.IsEnabled = false;
            btnDisableTask.IsEnabled = false;
            btnEnableTask.IsEnabled = false;
        }

        #endregion

        #region Checkbox Operations
        private void chbAllTasksChechbox_Checked(object sender, RoutedEventArgs e)
        {
            _tasksAllControl = true;

            foreach (TaskInfo item in listViewTasks.ItemsSource)
            {
                listViewTasks.SelectedItems.Add(item);
            }

            if (listViewTasks.SelectedItems.Count == 1)
            {
                btnTaskEdit.IsEnabled = true;
                btnTaskStart.IsEnabled = true;
                // çalışan görevi start etme engellendi

                ToggleTaskButtons();
            }
            else
            {
                btnTaskEdit.IsEnabled = false;
                btnTaskStart.IsEnabled = false;
            }

            if (listViewTasks.SelectedItems.Count > 0)
            {
                btnTaskDelete.IsEnabled = true;
                //btnTaskStart.IsEnabled = true;
            }
            else
            {
                btnTaskDelete.IsEnabled = false;
                //btnTaskStart.IsEnabled = false;
            }
        }

        private void chbAllTasksChechbox_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_tasksAllControl)
            {
                foreach (TaskInfo item in listViewTasks.ItemsSource)
                {
                    listViewTasks.SelectedItems.Remove(item);
                }
                _tasksAllControl = true;
            }

            if (listViewTasks.SelectedItems.Count == 1)
            {
                btnTaskEdit.IsEnabled = true;
                btnTaskStart.IsEnabled = true;
                // çalışan görevi start etme engellendi
                ToggleTaskButtons();
            }
            else
            {
                btnTaskEdit.IsEnabled = false;
                btnTaskStart.IsEnabled = false;
            }

            if (listViewTasks.SelectedItems.Count > 0)
            {
                btnTaskDelete.IsEnabled = true;
                //btnTaskStart.IsEnabled = true;
            }
            else
            {
                btnTaskDelete.IsEnabled = false;
                //btnTaskStart.IsEnabled = false;
            }
        }

        private void chbTask_Checked(object sender, RoutedEventArgs e)
        {
            if (chbAllTasksChechbox.IsChecked == false)
            {
                _tasksAllControl = listViewTasks.SelectedItems.Count == listViewTasks.Items.Count;
                chbAllTasksChechbox.IsChecked = _tasksAllControl;
            }

            if (listViewTasks.SelectedItems.Count == 1)
            {
                btnTaskEdit.IsEnabled = true;
                btnTaskStart.IsEnabled = true;
                // çalışan görevi start etme engellendi
                ToggleTaskButtons();
            }
            else
            {
                btnTaskEdit.IsEnabled = false;
                btnTaskStart.IsEnabled = false;
            }

            if (listViewTasks.SelectedItems.Count > 0)
                btnTaskDelete.IsEnabled = true;
            else
                btnTaskDelete.IsEnabled = false;
        }

        private void chbTask_Unchecked(object sender, RoutedEventArgs e)
        {
            _tasksAllControl = false;
            chbAllTasksChechbox.IsChecked = false;

            if (listViewTasks.SelectedItems.Count == 1)
            {
                btnTaskEdit.IsEnabled = true;
                btnTaskStart.IsEnabled = true;
                // çalışan görevi start etme engellendi
                ToggleTaskButtons();
            }
            else
            {
                btnTaskEdit.IsEnabled = false;
                btnTaskStart.IsEnabled = false;
            }

            if (listViewTasks.SelectedItems.Count > 0)
                btnTaskDelete.IsEnabled = true;
            else
                btnTaskDelete.IsEnabled = false;
        }
        #endregion

        #endregion


        #region Restore Tab

        private void btnRestore_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnRestore_Click istekte bulunuldu");

            List<VolumeInfo> volumeInfoList = new List<VolumeInfo>();
            foreach (VolumeInfo item in listViewRestoreDisk.SelectedItems)
            {
                volumeInfoList.Add(item);
            }

            BackupInfo backupInfo = (BackupInfo)listViewRestore.SelectedItem;
            bool controlFlag = false;
            if (volumeInfoList.Count > 1)
            {
                //disk kontrol et
                foreach (var item in _diskList)
                {
                    if (item.VolumeInfos[0].DiskName.Equals(volumeInfoList[0].DiskName))
                    {
                        if (backupInfo.UsedSize > item.Size)
                        {
                            controlFlag = true;
                            MessageBox.Show("Bu restore boyutlardan dolayı gerçekleştirilemez.", "Narbulut diyor ki; ", MessageBoxButton.OK, MessageBoxImage.Error);
                        }
                    }
                }
            }
            else
            {
                if (backupInfo.UsedSize > volumeInfoList[0].Size)
                {
                    controlFlag = true;
                    MessageBox.Show("Bu restore boyutlardan dolayı gerçekleştirilemez.", "Narbulut diyor ki; ", MessageBoxButton.OK, MessageBoxImage.Error);
                }

            }

            if (!controlFlag)
            {
                using (var scope = _scope.BeginLifetimeScope())
                {
                    RestoreWindow restore = scope.Resolve<RestoreWindow>(new TypedParameter(backupInfo.GetType(), backupInfo),
                        new TypedParameter(volumeInfoList.GetType(), volumeInfoList));
                    restore.ShowDialog();
                }
            }
            GetTasks();

        }

        private void listViewRestore_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewRestore.SelectedIndex != -1)
            {
                if (listViewRestoreDisk.SelectedIndex != -1)
                    btnRestore.IsEnabled = true;
                /*
                 * BAKILACAK*/
                BackupInfo backupInfo = (BackupInfo)listViewRestore.SelectedItem;

                txtRDriveLetter.Text = backupInfo.Letter.ToString();
                txtROS.Text = backupInfo.OS;
                if (backupInfo.DiskType.Equals("M"))
                    txtRBootPartition.Text = "MBR";
                else
                    txtRBootPartition.Text = "GPT";

                txtRVolumeSize.Text = backupInfo.StrVolumeSize;
                txtRUsedSize.Text = backupInfo.StrUsedSize;

                txtRDriveLetter.Text = backupInfo.Letter.ToString();
                txtRCreatedDate.Text = backupInfo.CreatedDate;
                txtRPcName.Text = backupInfo.PCName;
                txtRIpAddress.Text = backupInfo.IpAddress;
                txtRFolderSize.Text = backupInfo.StrFileSize;
                //Console.WriteLine("dosya adı : " + backupInfo.FileName);
            }
            else
            {
                btnRestore.IsEnabled = false;
            }
        }

        private void listViewRestoreDisk_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewRestoreDisk.SelectedIndex != -1)
            {
                if (listViewRestore.SelectedIndex != -1)
                    btnRestore.IsEnabled = true;
            }
            else
            {
                btnRestore.IsEnabled = false;
            }
        }

        private void Expander_Loaded_1(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("Expander_Loaded_1 istekte bulunuldu");

            var expander = sender as Expander;
            _expanderRestoreDiskList.Add(expander);
            var size = expander.FindName("txtRestoreTotalSize") as TextBlock;
            var expanderCheck = expander.FindName("cbRestoreHeader") as CheckBox;
            _restoreExpanderCheckBoxes.Add(expanderCheck);
            var numberOfItems = expander.FindName("txtRestoreNumberOfItems") as TextBlock;
            _restoreNumberOfItems.Add(Convert.ToInt32(numberOfItems.Text));
            var groupName = expander.FindName("txtRestoreGroupName") as TextBlock;
            _restoreGroupName.Add(groupName.Text);
            if (_diskList.Count == _diskExpenderIndex)
                _diskExpenderIndex = 0;
            size.Text = FormatBytes(_diskList[_diskExpenderIndex++].Size);
        }


        #region Checkbox Operations

        private void chbRestoreDisk_Checked(object sender, RoutedEventArgs e)
        {
            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as VolumeInfo; //data seçilen değer

            for (int i = 0; i < _restoreGroupName.Count; i++)
            {
                if (data.DiskName.Equals(_restoreGroupName[i]))
                {
                    int totalSelected = 0;
                    //i kaçıncı sıradaki adete eşit olacağı
                    foreach (VolumeInfo item in listViewRestoreDisk.SelectedItems)
                    {
                        if (item.DiskName.Equals(data.DiskName))
                        {
                            totalSelected++;
                        }
                    }
                    if (_restoreNumberOfItems[i] == totalSelected)
                    {
                        _restoreExpanderCheckBoxes[i].IsChecked = true;
                    }
                }
            }
        }

        private void chbRestoreDisk_Unchecked(object sender, RoutedEventArgs e)
        {
            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as VolumeInfo; //data seçilen değer

            for (int i = 0; i < _restoreGroupName.Count; i++)
            {
                if (_restoreGroupName[i].Equals(data.DiskName))
                {
                    _restoreExpanderCheckBoxes[i].IsChecked = false;
                }
            }
        }

        private void cbRestoreHeader_Checked(object sender, RoutedEventArgs e)
        {
            var headerCheckBox = sender as CheckBox;

            listViewRestoreDisk.SelectionMode = SelectionMode.Multiple;

            foreach (VolumeInfo item in listViewRestoreDisk.Items)
            {
                if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewRestoreDisk.SelectedItems.Add(item);
                }
                else
                {
                    listViewRestoreDisk.SelectedItems.Remove(item);
                }
            }
            for (int i = 0; i < _expanderRestoreDiskList.Count; i++)
            {
                if ((bool)!(_restoreExpanderCheckBoxes[i].IsChecked))
                {
                    _expanderRestoreDiskList[i].IsExpanded = false;
                    _expanderRestoreDiskList[i].IsEnabled = false;
                }
            }
        }

        private void cbRestoreHeader_Unchecked(object sender, RoutedEventArgs e)
        {
            var headerCheckBox = sender as CheckBox;

            foreach (VolumeInfo item in listViewRestoreDisk.Items)
            {
                if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewRestoreDisk.SelectedItems.Remove(item);
                }
            }

            _restoreExpanderCheckBoxes.ForEach(cb => cb.IsEnabled = true);
            foreach (var item in _expanderRestoreDiskList)
            {
                item.IsExpanded = true;
                item.IsEnabled = true;
            }

            listViewRestoreDisk.SelectionMode = SelectionMode.Single;
        }

        #endregion


        #endregion


        #region View Backups Tab

        private void btnFilesBrowse_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnFilesBrowse_Click istekte bulunuldu");

            using (var scope = _scope.BeginLifetimeScope())
            {
                var backupInfo = (BackupInfo)listViewBackups.SelectedItem;
                FileExplorerWindow fileExplorer = scope.Resolve<FileExplorerWindow>(new TypedParameter(backupInfo.GetType(), backupInfo));
                fileExplorer.ShowDialog();
            }
        }

        private void listViewBackups_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            _logger.Verbose("listViewBackups_MouseDoubleClick istekte bulunuldu");

            if (listViewBackups.SelectedIndex != -1)
            {
                using (var scope = _scope.BeginLifetimeScope())
                {
                    var backupInfo = (BackupInfo)listViewBackups.SelectedItem;
                    FileExplorerWindow fileExplorer = scope.Resolve<FileExplorerWindow>(new TypedParameter(backupInfo.GetType(), backupInfo));
                    fileExplorer.ShowDialog();
                }
            }
        }

        private void listViewBackups_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewBackups.SelectedIndex != -1)
            {
                btnFilesDelete.IsEnabled = true;
                btnFilesBrowse.IsEnabled = true;
            }
        }

        #region Checkbox Operations
        private void chbAllFilesChecbox_Checked(object sender, RoutedEventArgs e)
        {
            _viewBackupsAllControl = true;

            foreach (BackupInfo item in listViewBackups.ItemsSource)
            {
                listViewBackups.SelectedItems.Add(item);
            }

            if (listViewBackups.SelectedItems.Count == 1)
                btnFilesBrowse.IsEnabled = true;
            else
                btnFilesBrowse.IsEnabled = false;

            if (listViewBackups.SelectedItems.Count > 0)
                btnFilesDelete.IsEnabled = true;
            else
                btnFilesDelete.IsEnabled = false;
        }

        private void chbAllFilesChecbox_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_viewBackupsAllControl)
            {
                foreach (BackupInfo item in listViewBackups.ItemsSource)
                {
                    listViewBackups.SelectedItems.Remove(item);
                }
                _viewBackupsAllControl = true;
            }

            if (listViewBackups.SelectedItems.Count == 1)
                btnFilesBrowse.IsEnabled = true;
            else
                btnFilesBrowse.IsEnabled = false;

            if (listViewBackups.SelectedItems.Count > 0)
                btnFilesDelete.IsEnabled = true;
            else
                btnFilesDelete.IsEnabled = false;
        }

        private void chbViewBackups_Checked(object sender, RoutedEventArgs e)
        {
            if (chbAllFilesChecbox.IsChecked == false)
            {
                _viewBackupsAllControl = listViewBackups.SelectedItems.Count == listViewBackups.Items.Count;
                chbAllFilesChecbox.IsChecked = _viewBackupsAllControl;
            }

            if (listViewBackups.SelectedItems.Count == 1)
                btnFilesBrowse.IsEnabled = true;
            else
                btnFilesBrowse.IsEnabled = false;

            if (listViewBackups.SelectedItems.Count > 0)
                btnFilesDelete.IsEnabled = true;
            else
                btnFilesDelete.IsEnabled = false;
        }

        private void chbViewBackups_Unchecked(object sender, RoutedEventArgs e)
        {
            _viewBackupsAllControl = false;
            chbAllFilesChecbox.IsChecked = false;

            if (listViewBackups.SelectedItems.Count == 1)
                btnFilesBrowse.IsEnabled = true;
            else
                btnFilesBrowse.IsEnabled = false;

            if (listViewBackups.SelectedItems.Count > 0)
                btnFilesDelete.IsEnabled = true;
            else
                btnFilesDelete.IsEnabled = false;
        }
        #endregion


        #endregion


        #region Backup Storage Tab

        public static List<BackupStorageInfo> GetBackupStorages(List<VolumeInfo> volumeList, List<BackupStorageInfo> backupStorageInfoList)
        {
            //List<BackupStorageInfo> backupStorageInfoList = _backupStorageService.BackupStorageInfoList();
            string backupStorageLetter;

            foreach (var storageItem in backupStorageInfoList)
            {
                backupStorageLetter = storageItem.Path.Substring(0, storageItem.Path.Length - (storageItem.Path.Length - 1));

                foreach (var volumeItem in volumeList)
                {
                    if (backupStorageLetter.Equals(Convert.ToString(volumeItem.Letter)))
                    {
                        storageItem.StrCapacity = volumeItem.StrSize;
                        storageItem.StrFreeSize = volumeItem.StrFreeSize;
                        storageItem.StrUsedSize = FormatBytes(volumeItem.Size - volumeItem.FreeSize);
                        storageItem.Capacity = volumeItem.Size;
                        storageItem.FreeSize = volumeItem.FreeSize;
                        storageItem.UsedSize = volumeItem.Size - volumeItem.FreeSize;
                    }
                }

                if (storageItem.IsCloud) // cloud bilgileri alınıp hibritse burada doldurulacak
                {
                    storageItem.CloudCapacity = 107374182400;
                    storageItem.CloudUsedSize = 21474836480;
                    storageItem.CloudFreeSize = 85899345920;
                    storageItem.StrCloudCapacity = FormatBytes(storageItem.CloudCapacity);
                    storageItem.StrCloudUsedSize = FormatBytes(storageItem.CloudUsedSize);
                    storageItem.StrCloudFreeSize = FormatBytes(storageItem.CloudFreeSize);
                }
            }

            return backupStorageInfoList;
        }

        private void btnBackupStorageAdd_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnBackupStorageAdd_Click istekte bulunuldu");
            using (var scope = _scope.BeginLifetimeScope())
            {
                AddBackupAreaWindow addBackupArea = scope.Resolve<AddBackupAreaWindow>();
                addBackupArea.ShowDialog();
                var backupStorageService = _scope.Resolve<IBackupStorageService>();
                _logger.Verbose("GetBackupStorages istekte bulunuldu");
                listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, backupStorageService.BackupStorageInfoList());
                chbAllBackupStorage.IsChecked = true;
                chbAllBackupStorage.IsChecked = false;
            }
        }

        private void btnBackupStorageEdit_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnBackupStorageEdit_Click istekte bulunuldu");
            using (var scope = _scope.BeginLifetimeScope())
            {
                BackupStorageInfo backupStorageInfo = (BackupStorageInfo)listViewBackupStorage.SelectedItem;
                AddBackupAreaWindow addBackupArea = scope.Resolve<AddBackupAreaWindow>(new TypedParameter(backupStorageInfo.GetType(), backupStorageInfo));
                addBackupArea.ShowDialog();
                var backupStorageService = _scope.Resolve<IBackupStorageService>();
                _logger.Verbose("GetBackupStorages istekte bulunuldu");
                listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, backupStorageService.BackupStorageInfoList());
                chbAllBackupStorage.IsChecked = true;
                chbAllBackupStorage.IsChecked = false;
            }
        }

        private void btnBackupStorageDelete_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnBackupStorageDelete_Click istekte bulunuldu");

            MessageBoxResult result = MessageBox.Show($"{listViewBackupStorage.SelectedItems.Count} adet veri silinecek. Onaylıyor musunuz?", "Narbulut diyor ki; ", MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            if (result == MessageBoxResult.Yes)
            {
                var backupStorageService = _scope.Resolve<IBackupStorageService>();
                foreach (BackupStorageInfo item in listViewBackupStorage.SelectedItems)
                {
                    backupStorageService.DeleteBackupStorage(item);
                }
                _logger.Verbose("GetBackupStorages istekte bulunuldu");
                listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, backupStorageService.BackupStorageInfoList());
            }
        }

        private void listViewBackupStorage_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewBackupStorage.SelectedIndex != -1)
            {
                btnBackupStorageDelete.IsEnabled = true;
                btnBackupStorageEdit.IsEnabled = true;
            }
        }

        #region Checkbox Operations
        private void chbAllBackupStorage_Checked(object sender, RoutedEventArgs e)
        {
            _storegeAllControl = true;

            foreach (BackupStorageInfo item in listViewBackupStorage.ItemsSource)
            {
                listViewBackupStorage.SelectedItems.Add(item);
            }

            if (listViewBackupStorage.SelectedItems.Count == 1)
            {
                btnBackupStorageEdit.IsEnabled = true;
            }
            else
            {
                btnBackupStorageEdit.IsEnabled = false;
            }
            if (listViewBackupStorage.SelectedItems.Count > 0)
            {
                btnBackupStorageDelete.IsEnabled = true;
            }
            else
            {
                btnBackupStorageDelete.IsEnabled = false;
            }
        }

        private void chbAllBackupStorage_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_storegeAllControl)
            {
                foreach (BackupStorageInfo item in listViewBackupStorage.ItemsSource)
                {
                    listViewBackupStorage.SelectedItems.Remove(item);
                }
                _storegeAllControl = true;
            }

            if (listViewBackupStorage.SelectedItems.Count == 1)
            {
                btnBackupStorageEdit.IsEnabled = true;
            }
            else
            {
                btnBackupStorageEdit.IsEnabled = false;
            }
            if (listViewBackupStorage.SelectedItems.Count > 0)
            {
                btnBackupStorageDelete.IsEnabled = true;
            }
            else
            {
                btnBackupStorageDelete.IsEnabled = false;
            }
        }

        private void chbBackupStorage_Checked(object sender, RoutedEventArgs e)
        {
            if (chbAllBackupStorage.IsChecked == false)
            {
                _storegeAllControl = listViewBackupStorage.SelectedItems.Count == listViewBackupStorage.Items.Count;
                chbAllBackupStorage.IsChecked = _storegeAllControl;
            }

            if (listViewBackupStorage.SelectedItems.Count == 1)
            {
                btnBackupStorageEdit.IsEnabled = true;
            }
            else
            {
                btnBackupStorageEdit.IsEnabled = false;
            }
            if (listViewBackupStorage.SelectedItems.Count > 0)
            {
                btnBackupStorageDelete.IsEnabled = true;
            }
            else
            {
                btnBackupStorageDelete.IsEnabled = false;
            }
        }

        private void chbBackupStorage_Unchecked(object sender, RoutedEventArgs e)
        {
            _storegeAllControl = false;
            chbAllBackupStorage.IsChecked = false;

            if (listViewBackupStorage.SelectedItems.Count == 1)
            {
                btnBackupStorageEdit.IsEnabled = true;
            }
            else
            {
                btnBackupStorageEdit.IsEnabled = false;
            }
            if (listViewBackupStorage.SelectedItems.Count > 0)
            {
                btnBackupStorageDelete.IsEnabled = true;
            }
            else
            {
                btnBackupStorageDelete.IsEnabled = false;
            }
        }

        #endregion


        #endregion


        #region Log Tab

        private void listViewLog_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewLog.SelectedIndex != -1)
                btnLogDelete.IsEnabled = true;
            else
                btnLogDelete.IsEnabled = false;
        }

        private void listViewLog_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            _logger.Verbose("listViewLog_MouseDoubleClick istekte bulunuldu");

            if (listViewLog.SelectedIndex != -1)
            {
                ActivityLog activityLog = (ActivityLog)listViewLog.SelectedItem;
                activityLog.StatusInfo = _statusInfoDal.Get(x => x.Id == activityLog.StatusInfoId);
                StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("statusInfo", activityLog.StatusInfo));
                backupStatus.Show();
            }
        }

        private void btnLogDelete_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnLogDelete_Click istekte bulunuldu");

            MessageBoxResult result = MessageBox.Show($"{((ActivityLog)listViewLog.SelectedItem).TaskInfoName} ile ilgili veri silinecek. Onaylıyor musunuz?", "Narbulut diyor ki; ", MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            if (result == MessageBoxResult.Yes)
            {
                foreach (ActivityLog item in listViewLog.SelectedItems)
                {
                    _statusInfoDal.Delete(_statusInfoDal.Get(x => x.Id == item.StatusInfoId));
                    _activityLogDal.Delete(item);
                }
                ShowActivityLog();
            }
        }

        private void ShowActivityLog()
        {
            _logger.Verbose("ShowActivityLog metoduna istekte bulunuldu");

            _activityLogList = _activityLogDal.GetList();
            foreach (var item in _activityLogList)
            {
                item.StrStatus = Resources[$"{item.StrStatus}"].ToString();
            }

            listViewLog.ItemsSource = _activityLogList;
            CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewLog.ItemsSource);
            view.SortDescriptions.Add(new SortDescription("Id", ListSortDirection.Descending));
        }

        #endregion


        #region Settings Tab

        #region Upload
        private void btnUploadDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtUpload.Text);
            if (count != 0)
            {
                count -= 1;
                txtUpload.Text = count.ToString();
            }
        }

        private void btnUploadUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtUpload.Text);
            if (count != 999)
            {
                count += 1;
                txtUpload.Text = count.ToString();
            }
        }
        private void rbtnUpload_Checked(object sender, RoutedEventArgs e)
        {
            stackUpload.IsEnabled = true;
        }

        private void rbtnUpload_Unchecked(object sender, RoutedEventArgs e)
        {
            stackUpload.IsEnabled = false;
        }
        #endregion

        #region Download
        private void btnDownloadDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtDownload.Text);
            if (count != 0)
            {
                count -= 1;
                txtDownload.Text = count.ToString();
            }
        }

        private void btnDownloadUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtDownload.Text);
            if (count != 999)
            {
                count += 1;
                txtDownload.Text = count.ToString();
            }
        }
        private void rbtnDownload_Checked(object sender, RoutedEventArgs e)
        {
            stackDownload.IsEnabled = true;
        }

        private void rbtnDownload_Unchecked(object sender, RoutedEventArgs e)
        {
            stackDownload.IsEnabled = false;
        }
        #endregion



        #endregion


        #region Help Tab
        private void OnNavigate(object sender, RequestNavigateEventArgs e)
        {
            if (e.Uri.IsAbsoluteUri)
                Process.Start(e.Uri.AbsoluteUri);

            e.Handled = true;
        }

        #endregion


        public async void RefreshTasks(CancellationToken cancellationToken, IBackupService backupService)
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    await Task.Delay(500, cancellationToken);
                    _logger.Verbose("RefreshTasks istekte bulunuldu");
                    //var backupService = _scope.Resolve<IBackupService>();

                    /*_diskList = backupService.GetDiskList();

                    foreach (var diskItem in _diskList)
                    {
                        foreach (var volumeItem in diskItem.VolumeInfos)
                        {
                            volumeItem.StrFreeSize += "+ yeniledi";
                            _volumeList.Add(volumeItem);
                        }
                    }

                    CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewDisk.ItemsSource);
                    view.Refresh();*/

                    // disk pageleri yeniliyor sorunsuz
                    GetDiskPage();

                    //log down
                    RefreshActivityLogDown(backupService);

                    // son yedekleme bilgisi
                    RefreshMiddleTaskDate();

                    // Ortadaki statu
                    RefreshMiddleTaskStatu();

                    //backupları yeniliyor
                    if (backupService.GetRefreshIncDiffTaskFlag())
                    {
                        _logger.Verbose("RefreshTasks: Task listesi ve backuplar yenileniyor");

                        RefreshBackupsandTasks(backupService);

                        backupService.RefreshIncDiffTaskFlag(false);
                    }

                    //activity logu yeniliyor
                    RefreshActivityLog(backupService);
                }
                catch (Exception e)
                {
                    MessageBox.Show(e.Message);
                }

            }
        }

        private void RefreshActivityLogDown(IBackupService backupService)
        {
            List<ActivityDownLog> logList = new List<ActivityDownLog>();
            /*logList.Add(new ActivityDownLog
            {
                Detail = "Log yazısı",
                Time = DateTime.Now.ToString()
            });*/
            logList = backupService.GetDownLogList();
            if (logList != null)
            {
                _logger.Verbose("RefreshTasks: ActivityLog down yenileniyor");

                foreach (ActivityDownLog item in logList)
                {
                    _logList.Add(item);
                }

                listViewLogDown.Items.Refresh();
            }
        }

        private void RefreshMiddleTaskDate()
        {
            if (listViewLog.Items.Count > 0)
            {
                _logger.Verbose("RefreshTasks: Son başarılı-başarısız tarih yazdırılıyor");

                ActivityLog lastLog = ((ActivityLog)listViewLog.Items[0]);
                txtRunningStateBlock.Text = lastLog.EndDate.ToString();
                if (lastLog.Status == StatusType.Success)
                    txtRunningStateBlock.Foreground = Brushes.Green;
                else
                    txtRunningStateBlock.Foreground = Brushes.Red;
            }
        }

        private void RefreshMiddleTaskStatu()
        {
            TaskInfo runningTask = _taskInfoDal.GetList(x => x.Status == TaskStatusType.Working).FirstOrDefault();
            TaskInfo pausedTask = _taskInfoDal.GetList(x => x.Status == TaskStatusType.Paused).FirstOrDefault();
            if (runningTask != null)
            {
                _logger.Verbose("RefreshTasks: Çalışan task yazdırılıyor");

                // çalışanı yazdır
                runningTask.StatusInfo = _statusInfoDal.Get(x => x.Id == runningTask.StatusInfoId);
                txtMakeABackup.Text = Resources["makeABackup"].ToString() + ", "
                    + FormatBytesNonStatic(runningTask.StatusInfo.DataProcessed)
                    + ", %" + Math.Round((runningTask.StatusInfo.DataProcessed * 100.0) / (runningTask.StatusInfo.TotalDataProcessed), 2).ToString();
            }
            else if (pausedTask != null)
            {
                _logger.Verbose("RefreshTasks: Durdurulan task yazdırılıyor");

                // durdurulanı yazdır
                pausedTask.StatusInfo = _statusInfoDal.Get(x => x.Id == pausedTask.StatusInfoId);
                txtMakeABackup.Text = Resources["backupStopped"].ToString() + ", "
                    + FormatBytesNonStatic(pausedTask.StatusInfo.DataProcessed)
                    + ", %" + Math.Round((pausedTask.StatusInfo.DataProcessed * 100.0) / (pausedTask.StatusInfo.TotalDataProcessed), 2).ToString();
            }
            else
                txtMakeABackup.Text = "";
        }

        private void RefreshBackupsandTasks(IBackupService backupService)
        {
            _logger.Verbose("Task listesi ve backuplar yenileniyor");

            int taskSelectedIndex = -1;
            if (listViewTasks.SelectedIndex != -1)
            {
                taskSelectedIndex = listViewTasks.SelectedIndex;
            }
            GetTasks();
            listViewTasks.SelectedIndex = taskSelectedIndex;

            _backupsItems = backupService.GetBackupFileList(_backupStorageDal.GetList());
            listViewBackups.ItemsSource = _backupsItems;
            listViewRestore.ItemsSource = _backupsItems;
        }

        private void RefreshActivityLog(IBackupService backupService)
        {
            if (backupService.GetRefreshIncDiffLogFlag())
            {
                RefreshBackupsandTasks(backupService);
                _logger.Verbose("RefreshTasks: Activitylog listesi yenileniyor");

                int logSelectedIndex = -1;
                if (listViewLog.SelectedIndex != -1)
                {
                    logSelectedIndex = listViewLog.SelectedIndex;
                }
                ShowActivityLog();
                listViewLog.SelectedIndex = logSelectedIndex + 1;
                backupService.RefreshIncDiffLogFlag(false);
            }
        }

        private static T FindParent<T>(DependencyObject dependencyObject) where T : DependencyObject
        {
            var parent = VisualTreeHelper.GetParent(dependencyObject);
            if (parent == null)
                return null;

            var parentT = parent as T;
            return parentT ?? FindParent<T>(parent);
        }

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

        private string FormatBytesNonStatic(long bytes)
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

        private void btnTaskEdit_Click(object sender, RoutedEventArgs e)
        {
            List<BackupStorageInfo> backupStorageInfoList = new List<BackupStorageInfo>();
            foreach (BackupStorageInfo item in listViewBackupStorage.Items)
            {
                backupStorageInfoList.Add(item);
            }

            List<VolumeInfo> volumeInfoList = new List<VolumeInfo>();
            foreach (VolumeInfo item in listViewDisk.SelectedItems)
            {
                volumeInfoList.Add(item);
            }

            TaskInfo taskInfo = (TaskInfo)listViewTasks.SelectedItem;
            //Console.WriteLine("Storage: " + taskInfo.BackupStorageInfoId + /*" - " + taskInfo.BackupStorageInfo.StorageName +*/ "\n" +
            //    "BackupTaskInfı: " + taskInfo.BackupTaskId + /*" - " + taskInfo.BackupTaskInfo.TaskName +*/ "\n" +
            //    "Status: " + taskInfo.StatusInfoId /* + " - " + taskInfo.StatusInfo.TaskName*/);

            using (var scope = _scope.BeginLifetimeScope())
            {
                NewCreateTaskWindow newCreateTask = scope.Resolve<NewCreateTaskWindow>(new TypedParameter(backupStorageInfoList.GetType(), backupStorageInfoList),
                    new TypedParameter(volumeInfoList.GetType(), volumeInfoList), new TypedParameter(taskInfo.GetType(), taskInfo));
                newCreateTask.ShowDialog();
            }
            GetTasks();
            var _backupStorageService = _scope.Resolve<IBackupStorageService>();
            _logger.Verbose("GetBackupStorages istekte bulunuldu");
            listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, _backupStorageService.BackupStorageInfoList());
        }
    }
}
