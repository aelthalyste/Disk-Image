using Autofac;
using Autofac.Core.Lifetime;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler;
using DiskBackup.TaskScheduler.Jobs;
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
        private bool _restoreDiskAllControl = false;
        private bool _restoreDiskAllHeaderControl = false;

        private bool _storegeAllControl = false;
        private bool _viewBackupsAllControl = false;
        private bool _tasksAllControl = false;

        private List<CheckBox> _expanderCheckBoxes = new List<CheckBox>();
        private List<int> _numberOfItems = new List<int>();
        private List<string> _groupName = new List<string>();
        private List<CheckBox> _restoreExpanderCheckBoxes = new List<CheckBox>();
        private List<int> _restoreNumberOfItems = new List<int>();
        private List<string> _restoreGroupName = new List<string>();
        private List<DiskInformation> _diskList = new List<DiskInformation>();
        private List<VolumeInfo> _volumeList = new List<VolumeInfo>();
        private List<TaskInfo> _taskInfoList = new List<TaskInfo>();
        private List<ActivityLog> _activityLogList = new List<ActivityLog>();
        private List<BackupInfo> _backupsItems = new List<BackupInfo>();

        private CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();

        private readonly IBackupService _backupService;
        private readonly IBackupStorageService _backupStorageService;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly ITaskSchedulerManager _taskSchedulerManager;
        private readonly IBackupTaskDal _backupTaskDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;

        private readonly ILifetimeScope _scope;

        public MainWindow(IBackupService backupService, IBackupStorageService backupStorageService, ILifetimeScope scope, ITaskInfoDal taskInfoDal,
            IBackupStorageDal backupStorageDal, ITaskSchedulerManager taskSchedulerManager, IBackupTaskDal backupTaskDal, IStatusInfoDal statusInfoDal, IActivityLogDal activityLogDal)
        {
            InitializeComponent();


            _backupService = backupService;
            _backupStorageService = backupStorageService;
            _taskInfoDal = taskInfoDal;
            _backupStorageDal = backupStorageDal;
            _backupTaskDal = backupTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskSchedulerManager = taskSchedulerManager;
            _activityLogDal = activityLogDal;

            _scope = scope;

            _taskSchedulerManager.InitShedulerAsync();

            if (!_backupService.InitTracker())
            {
                MessageBox.Show("Driver intialize edilemedi!", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
            }

            #region Disk Bilgileri

            _diskList = _backupService.GetDiskList();

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

            foreach (var item in _diskList)
            {
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

            #endregion


            #region Yedekleme alanları

            listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, _backupStorageService.BackupStorageInfoList());

            #endregion


            #region Görevler

            GetTasks();

            #endregion


            #region ActivityLog

            ShowActivityLog();

            #endregion


            #region Backup dosya bilgileri

            _backupsItems = _backupService.GetBackupFileList(_backupStorageDal.GetList());

            listViewBackups.ItemsSource = _backupsItems;
            listViewRestore.ItemsSource = _backupsItems;

            #endregion


            RefreshTasks(_cancellationTokenSource.Token);
            this.Closing += (sender, e) => _cancellationTokenSource.Cancel();

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
            listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, _backupStorageService.BackupStorageInfoList());
        }

        private void Expander_Loaded(object sender, RoutedEventArgs e)
        {
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
            MessageBoxResult result = MessageBox.Show($"{listViewTasks.SelectedItems.Count} adet veri silinecek. Onaylıyor musunuz?", "Narbulut diyor ki; ", MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            if (result == MessageBoxResult.Yes)
            {
                foreach (TaskInfo item in listViewTasks.SelectedItems)
                {
                    // ilgili triggerı sil
                    if (item.ScheduleId != null && !item.ScheduleId.Contains("Now"))
                    {
                        _taskSchedulerManager.DeleteJob(item.ScheduleId);
                    }

                    // task tipine göre ilgili task tablosundan bilgisini sil
                    if (item.Type == TaskType.Backup)
                    {
                        _backupTaskDal.Delete(_backupTaskDal.Get(x => x.Id == item.BackupTaskId));
                    }
                    else
                    {
                        //restore silme
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
            TaskInfo taskInfo = (TaskInfo)listViewTasks.SelectedItem;

            if (taskInfo.Status.Equals("Hazır"))
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
                        if (taskInfo.ScheduleId != null && !taskInfo.ScheduleId.Contains("Now"))
                        {
                            _taskSchedulerManager.RunNowTrigger(taskInfo.ScheduleId).Wait();
                        }
                        else
                        {
                            _taskSchedulerManager.BackupIncDiffNowJob(taskInfo).Wait();
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
                }
                Console.WriteLine("Backup bitti");
            }
            else if (taskInfo.Status.Equals("Durduruldu"))
            {
                _backupService.ResumeTask(taskInfo);
            }
            BackupIncDiffJob._refreshIncDiffTaskFlag = true;
        }

        private void btnTaskPause_Click(object sender, RoutedEventArgs e)
        {
            _backupService.PauseTask((TaskInfo)listViewTasks.SelectedItem);
            BackupIncDiffJob._refreshIncDiffTaskFlag = true;
        }

        private void btnTaskStop_Click(object sender, RoutedEventArgs e)
        {
            _backupService.CancelTask((TaskInfo)listViewTasks.SelectedItem);
            BackupIncDiffJob._refreshIncDiffTaskFlag = true;
        }

        private void GetTasks()
        {
            _taskInfoList = _taskInfoDal.GetList();

            foreach (var item in _taskInfoList)
            {
                item.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == item.BackupStorageInfoId);
            }
            listViewTasks.ItemsSource = _taskInfoList;
            DisableTaskButtons();
        }

        public async void RefreshTasks(CancellationToken cancellationToken)
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    await Task.Delay(500);

                    // son yedekleme bilgisi
                    if (listViewLog.Items.Count > 0)
                    {
                        ActivityLog lastLog = ((ActivityLog)listViewLog.Items[0]);
                        txtRunningStateBlock.Text = lastLog.EndDate.ToString();
                        if (lastLog.Status == StatusType.Success)
                            txtRunningStateBlock.Foreground = Brushes.Green;
                        else
                            txtRunningStateBlock.Foreground = Brushes.Red;
                    }

                    // Ortadaki statu
                    TaskInfo runningTask = _taskInfoDal.GetList(x => x.Status == "Çalışıyor").FirstOrDefault();
                    TaskInfo pausedTask = _taskInfoDal.GetList(x => x.Status == "Durduruldu").FirstOrDefault();
                    if (runningTask != null)
                    {
                        // çalışanı yazdır
                        runningTask.StatusInfo = _statusInfoDal.Get(x => x.Id == runningTask.StatusInfoId);
                        txtMakeABackup.Text = Resources["makeABackup"].ToString() + ", "
                            + FormatBytesNonStatic(runningTask.StatusInfo.DataProcessed)
                            + ", %" + Math.Round((runningTask.StatusInfo.DataProcessed * 100.0) / (runningTask.StatusInfo.TotalDataProcessed), 2).ToString();
                    }
                    else if (pausedTask != null)
                    {
                        // durdurulanı yazdır
                        pausedTask.StatusInfo = _statusInfoDal.Get(x => x.Id == pausedTask.StatusInfoId);
                        txtMakeABackup.Text = Resources["backupStopped"].ToString() + ", "
                            + FormatBytesNonStatic(pausedTask.StatusInfo.DataProcessed)
                            + ", %" + Math.Round((pausedTask.StatusInfo.DataProcessed * 100.0) / (pausedTask.StatusInfo.TotalDataProcessed), 2).ToString();
                    }
                    else
                        txtMakeABackup.Text = "";

                    if (BackupIncDiffJob._refreshIncDiffTaskFlag)
                    {
                        int taskSelectedIndex = -1;
                        if (listViewTasks.SelectedIndex != -1)
                        {
                            taskSelectedIndex = listViewTasks.SelectedIndex;
                        }
                        GetTasks();
                        listViewTasks.SelectedIndex = taskSelectedIndex;

                        _backupsItems = _backupService.GetBackupFileList(_backupStorageDal.GetList());
                        listViewBackups.ItemsSource = _backupsItems;
                        listViewRestore.ItemsSource = _backupsItems;

                        BackupIncDiffJob._refreshIncDiffTaskFlag = false;
                    }

                    if (BackupIncDiffJob._refreshIncDiffLogFlag)
                    {
                        int logSelectedIndex = -1;
                        if (listViewLog.SelectedIndex != -1)
                        {
                            logSelectedIndex = listViewLog.SelectedIndex;
                        }
                        ShowActivityLog();
                        listViewLog.SelectedIndex = logSelectedIndex + 1;
                        BackupIncDiffJob._refreshIncDiffLogFlag = false;
                    }
                }
                catch (Exception e)
                {
                    MessageBox.Show(e.Message);
                }
               
            }
        }

        #region Task Buttons


        private void ToggleTaskButtons()
        {
            bool runningFlag = false;
            bool pauseFlag = false;
            foreach (TaskInfo item in listViewTasks.SelectedItems)
            {
                if (item.Status.Equals("Çalışıyor"))
                {
                    runningFlag = true;
                }
                else if (item.Status.Equals("Durduruldu"))
                {
                    pauseFlag = true;
                }
                else if (item.Status.Equals("Hazır"))
                {
                    btnTaskPause.IsEnabled = false;
                    btnTaskStop.IsEnabled = false;
                    btnTaskStart.IsEnabled = true;
                }
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
        }
        private void DisableTaskButtons()
        {
            btnTaskClose.IsEnabled = false;
            btnTaskCopy.IsEnabled = false;
            btnTaskDelete.IsEnabled = false;
            btnTaskEdit.IsEnabled = false;
            btnTaskOpen.IsEnabled = false;
            btnTaskPaste.IsEnabled = false;
            btnTaskPause.IsEnabled = false;
            btnTaskStart.IsEnabled = false;
            btnTaskStop.IsEnabled = false;
        }

        private void PausedTaskButtons()
        {
            btnTaskStart.IsEnabled = true;
            btnTaskEdit.IsEnabled = false;
            btnTaskPause.IsEnabled = false;
            btnTaskStop.IsEnabled = true;
            //diğer butonlar gelecek
            btnTaskDelete.IsEnabled = false;
        }

        private void RunningTaskButtons()
        {
            btnTaskStart.IsEnabled = false;
            btnTaskEdit.IsEnabled = false; // çalışan görev düzenlenemez
            btnTaskPause.IsEnabled = true;
            btnTaskStop.IsEnabled = true;
            btnTaskDelete.IsEnabled = false;
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
            var expander = sender as Expander;

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
        /*Header çalışıyor normal checkboxlarda tıkandım*/
        //private void chbRestoreDiskSelectAll_Checked(object sender, RoutedEventArgs e)
        //{
        //    //if (!_restoreDiskAllControl)
        //    //{
        //    //    if (_restoreExpanderCheckBoxes.Count == 1) //bir disk varsa çalış yoksa çalışma
        //    //    {
        //    //        foreach (VolumeInfo item in listViewRestoreDisk.ItemsSource)
        //    //        {
        //    //            listViewRestoreDisk.SelectedItems.Add(item);
        //    //        }
        //    //        _restoreDiskAllControl = true;
        //    //        _restoreExpanderCheckBoxes.ForEach(cb => cb.IsChecked = true);
        //    //        _restoreDiskAllHeaderControl = true;
        //    //    }
        //    //    else
        //    //        chbRestoreDiskSelectAll.IsChecked = false;
        //    //}
        //}

        //private void chbRestoreDiskSelectAll_Unchecked(object sender, RoutedEventArgs e)
        //{
        //    //if (_restoreDiskAllControl)
        //    //{
        //    //    if (_restoreExpanderCheckBoxes.Count == 1) //bir disk varsa çalış yoksa çalışma
        //    //    {
        //    //        _restoreExpanderCheckBoxes.ForEach(cb => cb.IsChecked = false);
        //    //        listViewRestoreDisk.SelectedItems.Clear();
        //    //        _restoreDiskAllHeaderControl = false;
        //    //    }
        //    //}
        //}

        //private void chbRestoreDisk_Checked(object sender, RoutedEventArgs e)
        //{
        //    if (listViewRestoreDisk.SelectedItems.Count == 1)
        //    {
        //        if (chbRestoreDiskSelectAll.IsChecked == false)
        //        {
        //            _restoreDiskAllControl = listViewRestoreDisk.SelectedItems.Count == listViewRestoreDisk.Items.Count;
        //            chbRestoreDiskSelectAll.IsChecked = _restoreDiskAllControl;
        //        }

        //        var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
        //        var data = dataItem.DataContext as VolumeInfo; //data seçilen değer

        //        for (int i = 0; i < _restoreGroupName.Count; i++)
        //        {
        //            if (data.DiskName.Equals(_restoreGroupName[i]))
        //            {
        //                int totalSelected = 0;
        //                //i kaçıncı sıradaki adete eşit olacağı
        //                foreach (VolumeInfo item in listViewRestoreDisk.SelectedItems)
        //                {
        //                    if (item.DiskName.Equals(data.DiskName))
        //                    {
        //                        totalSelected++;
        //                    }
        //                }
        //                if (_restoreNumberOfItems[i] == totalSelected)
        //                {
        //                    _restoreExpanderCheckBoxes[i].IsChecked = true;
        //                }
        //            }
        //        }
        //        _restoreDiskAllHeaderControl = true;
        //        // diğerlerini disable et
        //        foreach (VolumeInfo item in listViewRestoreDisk.ItemsSource)
        //        {
        //            bool controlFlag = true;
        //            foreach (VolumeInfo selectedItem in listViewRestoreDisk.SelectedItems)
        //            {
        //                if (item.Letter == selectedItem.Letter)
        //                {
        //                    controlFlag = false;
        //                    break;
        //                }
        //            }
        //            //if (controlFlag)
        //                //item. False işlemi yapılmalı

        //        }
        //    }
        //}

        //private void chbRestoreDisk_Unchecked(object sender, RoutedEventArgs e)
        //{
        //    //_restoreDiskAllControl = false;
        //    //chbRestoreDiskSelectAll.IsChecked = false;
        //    //_restoreDiskAllHeaderControl = false;

        //    //var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
        //    //var data = dataItem.DataContext as VolumeInfo; //data seçilen değer

        //    //for (int i = 0; i < _restoreGroupName.Count; i++)
        //    //{
        //    //    if (_restoreGroupName[i].Equals(data.DiskName))
        //    //    {
        //    //        _restoreExpanderCheckBoxes[i].IsChecked = false;
        //    //    }
        //    //}
        //}

        //private void cbRestoreHeader_Checked(object sender, RoutedEventArgs e)
        //{
        //    if (!_restoreDiskAllHeaderControl)
        //    {
        //        var headerCheckBox = sender as CheckBox;
        //        _restoreDiskAllHeaderControl = true;

        //        foreach (VolumeInfo item in listViewRestoreDisk.Items)
        //        {
        //            if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
        //            {
        //                listViewRestoreDisk.SelectedItems.Add(item);
        //            }
        //        }
        //        foreach (var item in _restoreExpanderCheckBoxes)
        //        {
        //            if (item.IsChecked == false)
        //                item.IsEnabled = false;
        //        }
        //    }
        //}

        //private void cbRestoreHeader_Unchecked(object sender, RoutedEventArgs e)
        //{
        //    if (_restoreDiskAllHeaderControl)
        //    {
        //        var headerCheckBox = sender as CheckBox;
        //        foreach (VolumeInfo item in listViewRestoreDisk.Items)
        //        {
        //            if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
        //            {
        //                listViewRestoreDisk.SelectedItems.Remove(item);
        //            }
        //        }
        //        _restoreDiskAllHeaderControl = false;
        //        _restoreExpanderCheckBoxes.ForEach(cb => cb.IsEnabled = true);
        //    }
        //}
        /*Normal checkboxlar*/
        private void chbRestoreDiskSelectAll_Checked(object sender, RoutedEventArgs e)
        {
            if (!_restoreDiskAllControl)
            {
                foreach (VolumeInfo item in listViewRestoreDisk.ItemsSource)
                {
                    listViewRestoreDisk.SelectedItems.Add(item);
                }
                _restoreDiskAllControl = true;
                _restoreExpanderCheckBoxes.ForEach(cb => cb.IsChecked = true);
            }
        }

        private void chbRestoreDiskSelectAll_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_restoreDiskAllControl)
            {
                _restoreExpanderCheckBoxes.ForEach(cb => cb.IsChecked = false);
                listViewRestoreDisk.SelectedItems.Clear();
                _restoreDiskAllHeaderControl = false;
            }
        }

        private void chbRestoreDisk_Checked(object sender, RoutedEventArgs e)
        {
            if (chbRestoreDiskSelectAll.IsChecked == false)
            {
                _restoreDiskAllControl = listViewRestoreDisk.SelectedItems.Count == listViewRestoreDisk.Items.Count;
                chbRestoreDiskSelectAll.IsChecked = _restoreDiskAllControl;
            }

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
            _restoreDiskAllControl = false;
            chbRestoreDiskSelectAll.IsChecked = false;
            _restoreDiskAllHeaderControl = false;

            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as VolumeInfo; //data seçilen değer

            for (int i = 0; i < _restoreGroupName.Count; i++)
            {
                if (_restoreGroupName[i].Equals(data.DiskName))
                {
                    _restoreExpanderCheckBoxes[i].IsChecked = false;
                    _restoreDiskAllHeaderControl = true;
                }
            }
        }

        private void cbRestoreHeader_Checked(object sender, RoutedEventArgs e)
        {
            var headerCheckBox = sender as CheckBox;
            _restoreDiskAllHeaderControl = true;

            foreach (VolumeInfo item in listViewRestoreDisk.Items)
            {
                if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewRestoreDisk.SelectedItems.Add(item);
                }
            }
        }

        private void cbRestoreHeader_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_restoreDiskAllHeaderControl)
            {
                var headerCheckBox = sender as CheckBox;
                foreach (VolumeInfo item in listViewRestoreDisk.Items)
                {
                    if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                    {
                        listViewRestoreDisk.SelectedItems.Remove(item);
                    }
                }
                _restoreDiskAllHeaderControl = true;
            }
        }

        #endregion


        #endregion


        #region View Backups Tab

        private void btnFilesBrowse_Click(object sender, RoutedEventArgs e)
        {
            using (var scope = _scope.BeginLifetimeScope())
            {
                FileExplorerWindow fileExplorer = scope.Resolve<FileExplorerWindow>();
                fileExplorer.ShowDialog();
            }
        }

        private void listViewBackups_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            using (var scope = _scope.BeginLifetimeScope())
            {
                FileExplorerWindow fileExplorer = scope.Resolve<FileExplorerWindow>();
                fileExplorer.ShowDialog();
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
            using (var scope = _scope.BeginLifetimeScope())
            {
                AddBackupAreaWindow addBackupArea = scope.Resolve<AddBackupAreaWindow>();
                addBackupArea.ShowDialog();
                listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, _backupStorageService.BackupStorageInfoList());
                chbAllBackupStorage.IsChecked = true;
                chbAllBackupStorage.IsChecked = false;
            }
        }

        private void btnBackupStorageEdit_Click(object sender, RoutedEventArgs e)
        {
            using (var scope = _scope.BeginLifetimeScope())
            {
                BackupStorageInfo backupStorageInfo = (BackupStorageInfo)listViewBackupStorage.SelectedItem;
                AddBackupAreaWindow addBackupArea = scope.Resolve<AddBackupAreaWindow>(new TypedParameter(backupStorageInfo.GetType(), backupStorageInfo));
                addBackupArea.ShowDialog();
                listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, _backupStorageService.BackupStorageInfoList());
                chbAllBackupStorage.IsChecked = true;
                chbAllBackupStorage.IsChecked = false;
            }
        }

        private void btnBackupStorageDelete_Click(object sender, RoutedEventArgs e)
        {
            MessageBoxResult result = MessageBox.Show($"{listViewBackupStorage.SelectedItems.Count} adet veri silinecek. Onaylıyor musunuz?", "Narbulut diyor ki; ", MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            if (result == MessageBoxResult.Yes)
            {
                foreach (BackupStorageInfo item in listViewBackupStorage.SelectedItems)
                {
                    _backupStorageService.DeleteBackupStorage(item);
                }
                listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, _backupStorageService.BackupStorageInfoList());
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


        private void btnRestore_Click(object sender, RoutedEventArgs e)
        {
            using (var scope = _scope.BeginLifetimeScope())
            {
                RestoreWindow restore = scope.Resolve<RestoreWindow>();
                restore.ShowDialog();
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
    }
}
