using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
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

        private IBackupService _backupService = new BackupManager();
        private IBackupStorageService _backupStorageService = new BackupStorageManager();

        public MainWindow()
        {
            InitializeComponent();

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

            GetBackupStorages(_volumeList);


            #endregion


            TaskInfo taskInfo1 = new TaskInfo()
            {
                Name = "Sistem Yedekleme",
                Type = TaskType.Backup
            };
            TaskInfo taskInfo2 = new TaskInfo()
            {
                Name = "Geri Yükleme",
                Type = TaskType.Restore
            };
            BackupStorageInfo backupAreaInfo1 = new BackupStorageInfo()
            {
                StorageName = "Narbulut"
            };
            BackupStorageInfo backupAreaInfo2 = new BackupStorageInfo()
            {
                StorageName = "Disk 2"
            };

            List<ActivityLog> activityLogItems = new List<ActivityLog>();

            activityLogItems.Add(new ActivityLog()
            {
                Id = 0,
                StartDate = DateTime.Now - TimeSpan.FromDays(10),
                EndDate = DateTime.Now - TimeSpan.FromHours(10),
                //BackupType = BackupTypes.Diff,
                TaskInfo = taskInfo1,
                BackupStorageInfo = backupAreaInfo2,
                Status = StatusType.Success,
                StrStatus = Resources[StatusType.Success.ToString()].ToString()
            });
            activityLogItems.Add(new ActivityLog()
            {
                Id = 0,
                StartDate = DateTime.Now - TimeSpan.FromDays(9),
                EndDate = DateTime.Now - TimeSpan.FromHours(8),
                //BackupType = BackupTypes.Diff,
                TaskInfo = taskInfo1,
                BackupStorageInfo = backupAreaInfo1,
                Status = StatusType.Fail,
                StrStatus = Resources[StatusType.Fail.ToString()].ToString()
            });
            activityLogItems.Add(new ActivityLog()
            {
                Id = 0,
                StartDate = DateTime.Now - TimeSpan.FromDays(5),
                EndDate = DateTime.Now - TimeSpan.FromHours(5),
                //backupType = BackupType.Full,
                TaskInfo = taskInfo2,
                BackupStorageInfo = backupAreaInfo2,
                Status = StatusType.Success,
                StrStatus = Resources[StatusType.Success.ToString()].ToString()
            });

            listViewLog.ItemsSource = activityLogItems;
            //buraya kadar uyarlandı loader'da falan da düzen gerekecek


            List<BackupInfo> backupsItems = new List<BackupInfo>();

            backupsItems.Add(new BackupInfo()
            {
                Type = BackupTypes.Full,
                FileName = "Volume_C_Full_001",
                CreatedDate = DateTime.Now - TimeSpan.FromDays(5),
                BackupTaskName = "C Full",
                StrVolumeSize = "250 GB",
                StrFileSize = "12 GB",
                Description = "Yedek açıklaması"
            });
            backupsItems.Add(new BackupInfo()
            {
                Type = BackupTypes.Inc,
                FileName = "Volume_D_Inc_001",
                CreatedDate = DateTime.Now - TimeSpan.FromHours(5),
                BackupTaskName = "D Inc",
                StrVolumeSize = "510 GB",
                StrFileSize = "8 GB",
                Description = "Yedek açıklaması"
            });
            backupsItems.Add(new BackupInfo()
            {
                Type = BackupTypes.Diff,
                FileName = "Volume_D_Diff_001",
                IsCloud = true,
                CreatedDate = DateTime.Now - TimeSpan.FromHours(30),
                BackupTaskName = "D Diff",
                StrVolumeSize = "250 GB",
                StrFileSize = "18 GB",
                Description = "Yedek açıklaması"
            });
            backupsItems.Add(new BackupInfo()
            {
                Type = BackupTypes.Diff,
                FileName = "Volume_D_Diff_002",
                CreatedDate = DateTime.Now - TimeSpan.FromDays(15),
                BackupTaskName = "D Diff",
                StrVolumeSize = "120 GB",
                StrFileSize = "8 GB",
                Description = "Yedek açıklaması"
            });
            backupsItems.Add(new BackupInfo()
            {
                Type = BackupTypes.Full,
                FileName = "Volume_C_Full_002",
                IsCloud = true,
                CreatedDate = DateTime.Now - TimeSpan.FromDays(7),
                BackupTaskName = "C Full",
                StrVolumeSize = "120 GB",
                StrFileSize = "28 GB",
                Description = "Yedek açıklaması"
            });
            backupsItems.Add(new BackupInfo()
            {
                Type = BackupTypes.Full,
                FileName = "Volume_C_Full_003",
                CreatedDate = DateTime.Now - TimeSpan.FromDays(7),
                BackupTaskName = "C Full",
                StrVolumeSize = "120 GB",
                StrFileSize = "28 GB",
                Description = "Yedek açıklaması"
            });
            backupsItems.Add(new BackupInfo()
            {
                Type = BackupTypes.Inc,
                FileName = "Volume_D_Inc_002",
                IsCloud = true,
                CreatedDate = DateTime.Now - TimeSpan.FromHours(5),
                BackupTaskName = "D Inc",
                StrVolumeSize = "510 GB",
                StrFileSize = "4 GB",
                Description = "Yedek açıklaması"
            });

            listViewBackups.ItemsSource = backupsItems;

            listViewRestore.ItemsSource = backupsItems;
        }


        #region Title Bar
        private void btnMainClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
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
            NewCreateTaskWindow newCreateTask = new NewCreateTaskWindow(backupStorageInfoList);
            newCreateTask.ShowDialog();
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

        private void listViewTasks_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewTasks.SelectedIndex != -1)
            {
                btnTaskDelete.IsEnabled = true;
                btnTaskEdit.IsEnabled = true;
                //butonlar eklenmeye devam edecek burayada da checkboxlara da
            }
        }

        #region Checkbox Operations
        private void chbAllTasksChechbox_Checked(object sender, RoutedEventArgs e)
        {
            _tasksAllControl = true;

            foreach (TaskInfo item in listViewTasks.ItemsSource)
            {
                listViewTasks.SelectedItems.Add(item);
            }

            if (listViewTasks.SelectedItems.Count == 1)
                btnTaskEdit.IsEnabled = true;
            else
                btnTaskEdit.IsEnabled = false;

            if (listViewTasks.SelectedItems.Count > 0)
                btnTaskDelete.IsEnabled = true;
            else
                btnTaskDelete.IsEnabled = false;
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
                btnTaskEdit.IsEnabled = true;
            else
                btnTaskEdit.IsEnabled = false;

            if (listViewTasks.SelectedItems.Count > 0)
                btnTaskDelete.IsEnabled = true;
            else
                btnTaskDelete.IsEnabled = false;
        }

        private void chbTask_Checked(object sender, RoutedEventArgs e)
        {
            if (chbAllTasksChechbox.IsChecked == false)
            {
                _tasksAllControl = listViewTasks.SelectedItems.Count == listViewTasks.Items.Count;
                chbAllTasksChechbox.IsChecked = _tasksAllControl;
            }

            if (listViewTasks.SelectedItems.Count == 1)
                btnTaskEdit.IsEnabled = true;
            else
                btnTaskEdit.IsEnabled = false;

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
                btnTaskEdit.IsEnabled = true;
            else
                btnTaskEdit.IsEnabled = false;

            if (listViewTasks.SelectedItems.Count > 0)
                btnTaskDelete.IsEnabled = true;
            else
                btnTaskDelete.IsEnabled = false;
        }
        #endregion



        #endregion


        #region Restore Tab

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


        #endregion


        #region View Backups Tab


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

        private void GetBackupStorages(List<VolumeInfo> volumeList)
        {
            List<BackupStorageInfo> backupStorageInfoList = _backupStorageService.BackupStorageInfoList();
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

            listViewBackupStorage.ItemsSource = backupStorageInfoList;
        }

        private void btnBackupStorageAdd_Click(object sender, RoutedEventArgs e)
        {
            AddBackupAreaWindow addBackupArea = new AddBackupAreaWindow();
            addBackupArea.ShowDialog();
            GetBackupStorages(_volumeList);
            chbAllBackupStorage.IsChecked = true;
            chbAllBackupStorage.IsChecked = false;
        }

        private void btnBackupStorageEdit_Click(object sender, RoutedEventArgs e)
        {
            BackupStorageInfo backupStorageInfo = (BackupStorageInfo)listViewBackupStorage.SelectedItem;
            AddBackupAreaWindow addBackupArea = new AddBackupAreaWindow(backupStorageInfo);
            addBackupArea.ShowDialog();
            GetBackupStorages(_volumeList);
            chbAllBackupStorage.IsChecked = true;
            chbAllBackupStorage.IsChecked = false;
        }

        private void btnBackupStorageDelete_Click(object sender, RoutedEventArgs e)
        {
            MessageBoxResult result = MessageBox.Show($"{listViewBackupStorage.SelectedItems.Count} adet veri silinecek. Onaylıyor musunuz?", "Narbulut diyor ki; " , MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            if (result == MessageBoxResult.Yes)
            {
                foreach (BackupStorageInfo item in listViewBackupStorage.SelectedItems)
                {
                    _backupStorageService.DeleteBackupStorage(item);
                }
                GetBackupStorages(_volumeList);
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


        private void btnTaskOpen_Click(object sender, RoutedEventArgs e)
        {

        }

        private void btnTaskPaste_Click(object sender, RoutedEventArgs e)
        {
            StatusesWindow backupStatus = new StatusesWindow(0);
            backupStatus.Show();
        }

        private void btnFilesBrowse_Click(object sender, RoutedEventArgs e)
        {
            FileExplorerWindow fileExplorer = new FileExplorerWindow();
            fileExplorer.Show();
        }

        private void btnTaskCopy_Click(object sender, RoutedEventArgs e)
        {
            StatusesWindow restoreStatus = new StatusesWindow(1);
            restoreStatus.Show();
        }

        private void btnRestore_Click(object sender, RoutedEventArgs e)
        {
            RestoreWindow restore = new RestoreWindow();
            try
            {
                restore.ShowDialog();
            }
            catch (Exception ex)
            {
                //Bu düzeltilecek.... umarım :(

                MessageBox.Show("Geçmişe dönük restore yapılamaz." + ex.ToString(), "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                restore.Close();
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

        private string FormatBytes(long bytes)
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
