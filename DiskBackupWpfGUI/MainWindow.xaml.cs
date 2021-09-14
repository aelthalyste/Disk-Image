using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.Communication;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler;
using DiskBackupWpfGUI.Utils;
using Serilog;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Navigation;

namespace DiskBackupWpfGUI
{
    public partial class MainWindow : Window
    {
        private int _diskExpanderIndex = 0;

        private bool _diskAllControl = false;
        private bool _diskAllHeaderControl = false;
        private bool _diskCloneSourceDiskAllControl = false;
        private bool _diskCloneTargetDiskAllControl = false;
        private bool _storegeAllControl = false;
        private bool _viewBackupsAllControl = false;
        private bool _tasksAllControl = false;

        private List<CheckBox> _expanderCheckBoxes = new List<CheckBox>();
        private List<CheckBox> _restoreExpanderCheckBoxes = new List<CheckBox>();
        private List<Expander> _expanderRestoreDiskList = new List<Expander>();
        private List<CheckBox> _diskCloneSourceCheckBoxes = new List<CheckBox>();
        private List<CheckBox> _diskCloneTargetCheckBoxes = new List<CheckBox>();
        private List<Expander> _expanderDiskCloneSourceDiskList = new List<Expander>();
        private List<Expander> _expanderDiskCloneTargetDiskList = new List<Expander>();
        
        private List<int> _numberOfItems = new List<int>();
        private List<string> _groupName = new List<string>();
        private List<int> _restoreNumberOfItems = new List<int>();
        private List<string> _restoreGroupName = new List<string>();
        private List<int> _diskCloneSourceNumberOfItems = new List<int>();
        private List<string> _diskCloneSourceGroupName = new List<string>();
        private List<int> _diskCloneTargetNumberOfItems = new List<int>();
        private List<string> _diskCloneTargetGroupName = new List<string>();

        private List<DiskInformation> _diskList = new List<DiskInformation>();
        private List<VolumeInfo> _volumeList = new List<VolumeInfo>();
        private List<TaskInfo> _taskInfoList = new List<TaskInfo>();
        private List<ActivityLog> _activityLogList = new List<ActivityLog>();
        private List<BackupInfo> _backupsItems = new List<BackupInfo>();
        private List<ActivityDownLog> _logList = new List<ActivityDownLog>();

        private CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();
        CollectionView _view;

        private Dictionary<string, string> languages = new Dictionary<string, string>();

        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly IBackupTaskDal _backupTaskDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly IActivityLogDal _activityLogDal;
        private readonly IRestoreTaskDal _restoreTaskDal;
        private IConfigurationDataDal _configurationDataDal;
        private ILicenseService _licenseService;
        private readonly ILifetimeScope _scope;
        private readonly ILogger _logger;

        public MainWindow(ILifetimeScope scope, ITaskInfoDal taskInfoDal, IBackupStorageDal backupStorageDal, IBackupTaskDal backupTaskDal, IStatusInfoDal statusInfoDal, IActivityLogDal activityLogDal, ILogger logger, IRestoreTaskDal restoreTaskDal, IConfigurationDataDal configurationDataDal, ILicenseService licenseService, IEMailOperations emailOperations)
        {
            InitializeComponent();

            Console.WriteLine(DateTime.Now);

            _logger = logger.ForContext<MainWindow>();
            _backupStorageDal = backupStorageDal;
            _activityLogDal = activityLogDal;
            _backupTaskDal = backupTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;
            _restoreTaskDal = restoreTaskDal;
            _configurationDataDal = configurationDataDal;
            _scope = scope;
            _licenseService = licenseService;

            var backupService = _scope.Resolve<IBackupService>();
            var backupStorageService = _scope.Resolve<IBackupStorageService>();

            var languageConfiguration = _configurationDataDal.Get(x => x.Key == "lang");
            if (languageConfiguration == null)
            {
                languageConfiguration = new ConfigurationData { Key = "lang", Value = "tr" };
                _configurationDataDal.Add(languageConfiguration);
            }
            SetApplicationLanguage(languageConfiguration.Value);

            ReloadLanguages();

            cbLang.SelectedValue = languageConfiguration.Value;
            cbLang.SelectionChanged += cbLang_SelectionChanged;

            try
            {
                if (!backupService.GetInitTracker())
                {
                    _logger.Information("Driver intialize edilemedi.");
                    MessageBox.Show(Resources["driverNotInitializedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Driver intialize edilemedi.");
                MessageBox.Show(Resources["driverNotInitializedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }

            try
            {
                _logger.Verbose("GetDiskList metoduna istekte bulunuldu");
                _diskList = backupService.GetDiskList();

                foreach (var diskItem in _diskList)
                {
                    foreach (var volumeItem in diskItem.VolumeInfos)
                    {
                        if (volumeItem.HealthStatu == HealthSituation.Healthy)
                            volumeItem.Status = Resources["healthy"].ToString();
                        else
                            volumeItem.Status = Resources["unhealthy"].ToString();

                        _volumeList.Add(volumeItem);
                    }
                }
                Console.WriteLine("List doldurma üst satır list count = " + _volumeList.Count);
                listViewDisk.ItemsSource = _volumeList;
                listViewDiskCloneSource.ItemsSource = _volumeList;
                listViewDiskCloneTarget.ItemsSource = _volumeList;
                listViewRestoreDisk.ItemsSource = _volumeList;
                Console.WriteLine("List doldurma bitiş satırı diskDown = " + listViewDiskCloneSource.Items.Count);

                _view = (CollectionView)CollectionViewSource.GetDefaultView(listViewDisk.ItemsSource);
                PropertyGroupDescription groupDescription = new PropertyGroupDescription("DiskName");
                _view.GroupDescriptions.Add(groupDescription);

                GetDiskPage();
            }
            catch (Exception e)
            {
                _logger.Error(e, "Disk bilgileri getirilemedi!");
            }

            Initilaze();

            this.Closing += (sender, e) => _cancellationTokenSource.Cancel();
            Console.WriteLine("CTOR SON: " + DateTime.Now);
        }


        #region Lisans Kontrolleri

        private void LicenseController()
        {

            if (!_licenseService.ThereIsARegistryFile()) // dosya yok
            {
                if (!_licenseService.ThereIsAFile())
                {
                    _logger.Information("Lisans dosyası bulunamadı.");
                    txtLicenseNotActive.Visibility = Visibility.Visible;
                    txtLicenseStatu.Text = Resources["inactive"].ToString();
                    txtMachineType.Text = Resources[_licenseService.GetMachineType().ToString()].ToString();

                    LicenseControllerWindow licenseControllerWindow = _scope.Resolve<LicenseControllerWindow>(new NamedParameter("windowType", false));
                    licenseControllerWindow.ShowDialog();

                    if (!licenseControllerWindow._validate)
                    {
                        Close();
                    }
                    else
                    {
                        if (_licenseService.GetRegistryType() == "1505")
                        {
                            txtLicenseNotActive.Visibility = Visibility.Collapsed;
                            txtDemoDays.Text = _licenseService.GetDemoDaysLeft().ToString();
                            stackDemo.Visibility = Visibility.Visible;
                            txtLicenseStatu.Text = Resources["demo"].ToString();
                            var customer = _configurationDataDal.Get(x => x.Key == "customerName");
                            if (customer == null)
                                customer = _configurationDataDal.Add(new ConfigurationData { Key = "customerName", Value = "Demo Demo" });
                            txtCustomerName.Text = customer.Value;
                            //servisde de var
                            _licenseService.UpdateDemoLastDate();
                        }
                        else // lisans
                        {
                            txtLicenseNotActive.Visibility = Visibility.Collapsed;
                            LicenseInformationWrite(_licenseService.GetLicenseUserInfo());
                        }
                    }
                }
                else
                {
                    _logger.Error("Demo bilgileri bozulmuş, lisans girişi istenecek! (505)");
                    _licenseService.DeleteRegistryFile();
                    FixBrokenRegistry();
                }
            }
            else // dosya var
            {
                try
                {
                    if (_licenseService.GetRegistryType() == "1505") // gün kontrolleri yapılacak
                    {
                        if (_licenseService.IsDemoExpired()) //uygulama çalışabilir
                        {
                            txtDemoDays.Text = _licenseService.GetDemoDaysLeft().ToString();
                            stackDemo.Visibility = Visibility.Visible;
                            txtLicenseStatu.Text = Resources["demo"].ToString();
                            txtMachineType.Text = Resources[_licenseService.GetMachineType().ToString()].ToString();
                            var customer = _configurationDataDal.Get(x => x.Key == "customerName");
                            if (customer == null)
                                customer = _configurationDataDal.Add(new ConfigurationData { Key = "customerName", Value = "Demo Demo" });
                            txtCustomerName.Text = customer.Value;
                            //servisde de var
                            _licenseService.UpdateDemoLastDate();
                        }
                        else // deneme süresi doldu
                        {
                            _logger.Information("Demo lisans süresi doldu.");
                            if (stackDemo.Visibility == Visibility.Visible)
                                stackDemo.Visibility = Visibility.Collapsed;
                            txtLicenseNotActive.Visibility = Visibility.Visible;
                            txtLicenseStatu.Text = Resources["inactive"].ToString();
                            txtMachineType.Text = Resources[_licenseService.GetMachineType().ToString()].ToString();
                            FixBrokenRegistry(); // registry ile oynanmış
                        }
                    }
                    else if (_licenseService.GetRegistryType() == "2606") // lisanslı
                    {
                        try
                        {
                            LicenseInformationWrite(_licenseService.GetLicenseUserInfo());
                        }
                        catch (Exception)
                        {
                            _logger.Error("Lisans bilgileri bozulmuş, tekrardan lisans girişi istenecek!");
                            FixBrokenRegistry();
                        }
                    }
                    else
                    {
                        txtLicenseNotActive.Visibility = Visibility.Visible;
                        txtLicenseStatu.Text = Resources["inactive"].ToString();
                        txtMachineType.Text = Resources[_licenseService.GetMachineType().ToString()].ToString();
                        FixBrokenRegistry(); // registry ile oynanmış
                    }
                }
                catch (Exception)
                {
                    MessageBox.Show(Resources["unexpectedError1MB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    _logger.Error("Demo bilgileri bozulmuş, lisans girişi istenecek! (606)");
                    _licenseService.DeleteRegistryFile();
                    txtLicenseNotActive.Visibility = Visibility.Visible;
                    txtLicenseStatu.Text = Resources["inactive"].ToString();
                    txtMachineType.Text = Resources[_licenseService.GetMachineType().ToString()].ToString();
                    FixBrokenRegistry(); // registry ile oynanmış
                }
            }
        }

        private void LicenseInformationWrite(string[] splitLicenseKey)
        {
            _licenseService.AddDBCustomerNameAndUniqKey(_licenseService.GetLicenseKey());
            txtLicenseStatu.Text = Resources["active"].ToString();
            txtDealerName.Text = splitLicenseKey[0];
            txtCustomerName.Text = splitLicenseKey[1];
            txtAuthorizedPerson.Text = splitLicenseKey[2];
            txtMachineType.Text = Resources[splitLicenseKey[5]].ToString();
            txtVersionType.Text = splitLicenseKey[6];
            txtExpireDate.Text = Convert.ToDateTime(splitLicenseKey[4], CultureInfo.CreateSpecificCulture("tr-TR")).ToString();
            var supportDays = (Convert.ToDateTime(splitLicenseKey[4], CultureInfo.CreateSpecificCulture("tr-TR")) - DateTime.Now).Days;
            if (supportDays < 0)
                txtExpireDateDays.Text = "0";
            else
                txtExpireDateDays.Text = supportDays.ToString();
            stackSupportDate.Visibility = Visibility.Visible;
            txtVerificationKey.Text = splitLicenseKey[7];
        }

        private void FixBrokenRegistry()
        {
            _logger.Information("Lisans bilgileri değiştiriliyor.");
            _licenseService.FixBrokenRegistry();

            LicenseControllerWindow licenseControllerWindow = _scope.Resolve<LicenseControllerWindow>(new NamedParameter("windowType", true));
            licenseControllerWindow.ShowDialog(); // kontrol yolla demo seçeneğini kaldır
            if (!licenseControllerWindow._validate)
            {
                Close();
            }
            else
            {
                txtLicenseNotActive.Visibility = Visibility.Collapsed;
                LicenseInformationWrite(_licenseService.GetLicenseUserInfo());
            }
        }


        #endregion


        #region Async Listviewleri Doldurma Fonksiyonları

        private async void Initilaze()
        {
            mainTabControl.IsEnabled = false;
            txtMakeABackup.Text = Resources["loading..."].ToString();
            Console.WriteLine("Initilaze Baş : " + DateTime.Now);
            var backupService = _scope.Resolve<IBackupService>();
            Console.WriteLine("Backup storage: " + DateTime.Now);
            await GetBackupStoragesAsync(_volumeList);
            Console.WriteLine("Task: " + DateTime.Now);
            await GetTasksAsync();
            Console.WriteLine("Activity: " + DateTime.Now);
            await ShowActivityLogAsync();
            await ShowActivityLogDownAsync(backupService);
            Console.WriteLine("Backup Dosyaları getirilmeden önce: " + DateTime.Now);
            await ShowBackupsFilesAsync(backupService);
            Console.WriteLine("Email Ayarları getirilmeden önce: " + DateTime.Now);
            await GetEmailSettings();
            Console.WriteLine("Refresh: " + DateTime.Now);
            mainTabControl.IsEnabled = true;
            pbLoading.Visibility = Visibility.Collapsed;
            pbLoading.IsIndeterminate = false;
            RefreshTasks(_cancellationTokenSource.Token, backupService);
            Console.WriteLine("Initilaze son: " + DateTime.Now);
            LicenseController();
            RefreshDemoDays(_cancellationTokenSource.Token);
        }

        public async Task GetBackupStoragesAsync(List<VolumeInfo> volumeList)
        {
            _logger.Verbose("GetBackupStoragesAsync metoduna istekte bulunuldu");
            var backupStorageInfoList = await Task.Run(() =>
            {
                return _backupStorageDal.GetList();
            });
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

                if (storageItem.Type == BackupStorageType.NAS) //Nas boyut bilgisi hesaplama
                {
                    var _backupStorageService = _scope.Resolve<IBackupStorageService>();
                    var nasConnection = _backupStorageService.GetNasCapacityAndSize((storageItem.Path.Substring(0, storageItem.Path.Length - 1)), storageItem.Username, storageItem.Password, storageItem.Domain);
                    storageItem.Capacity = Convert.ToInt64(nasConnection[0]);
                    storageItem.UsedSize = Convert.ToInt64(nasConnection[0]) - Convert.ToInt64(nasConnection[1]);
                    storageItem.FreeSize = Convert.ToInt64(nasConnection[1]);
                    storageItem.StrCapacity = FormatBytes(Convert.ToInt64(nasConnection[0]));
                    storageItem.StrUsedSize = FormatBytes(Convert.ToInt64(nasConnection[0]) - Convert.ToInt64(nasConnection[1]));
                    storageItem.StrFreeSize = FormatBytes(Convert.ToInt64(nasConnection[1]));
                }
            }

            listViewBackupStorage.ItemsSource = backupStorageInfoList;
        }

        private async Task GetTasksAsync()
        {
            _logger.Verbose("GetTasksAsync metoduna istekte bulunuldu");
            var liste = await Task.Run(() =>
            {
                return _taskInfoDal.GetList();
            });
            _taskInfoList = liste;

            foreach (var item in _taskInfoList)
            {
                item.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == item.BackupStorageInfoId);
                item.StrStatus = item.Status.ToString();
                item.StrStatus = Resources[item.StrStatus].ToString();
            }
            listViewTasks.ItemsSource = _taskInfoList;
            DisableTaskButtons();
            Console.WriteLine("GetTasksAsync: " + DateTime.Now);
        }

        private async Task ShowActivityLogAsync()
        {
            _logger.Verbose("ShowActivityLogAsync metoduna istekte bulunuldu");

            _activityLogList = await Task.Run(() =>
            {
                return _activityLogDal.GetList();
            });

            foreach (var item in _activityLogList)
            {
                item.StatusInfo = _statusInfoDal.Get(x => x.Id == item.StatusInfoId);
                item.StatusInfo.StrStatus = Resources[$"{item.StatusInfo.StrStatus}"].ToString();
            }

            listViewLog.ItemsSource = _activityLogList;
            CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewLog.ItemsSource);
            view.SortDescriptions.Add(new SortDescription("Id", ListSortDirection.Descending));
        }

        private async Task ShowActivityLogDownAsync(IBackupService backupService)
        {
            _logger.Verbose("ShowActivityLogDownAsync metoduna istekte bulunuldu");
            try
            {
                _logList = await Task.Run(() =>
                {
                    return backupService.GetDownLogList();
                });
                listViewLogDown.ItemsSource = _logList;
            }
            catch (Exception e)
            {
                _logger.Error(e, "BackupService'den NARDIWrapper logları getirilemedi!");
            }
        }

        private async Task ShowBackupsFilesAsync(IBackupService backupService)
        {
            _logger.Verbose("ShowBackupsFilesAsync metoduna istekte bulunuldu");
            try
            {
                _backupsItems = await Task.Run(() =>
                {
                    return backupService.GetBackupFileList(_backupStorageDal.GetList());
                });
                string versions = "";
                foreach (var item in _backupsItems)
                {
                    versions += " " + item.Version.ToString(); 
                }
                listViewBackups.ItemsSource = _backupsItems;
                listViewRestore.ItemsSource = _backupsItems;
            }
            catch (Exception e)
            {
                _logger.Error(e, "Backup dosyaları getirilemedi!");
            }
        }

        public async Task GetEmailSettings()
        {
            _logger.Verbose("GetEmailSettings metoduna istekte bulunuldu");
            var emailActive = await Task.Run(() =>
            {
                return _configurationDataDal.Get(x => x.Key == "emailActive");
            });
            var emailSuccessful = await Task.Run(() =>
            {
                return _configurationDataDal.Get(x => x.Key == "emailSuccessful");
            });
            var emailFail = await Task.Run(() =>
            {
                return _configurationDataDal.Get(x => x.Key == "emailFail");
            });
            var emailCritical = await Task.Run(() =>
            {
                return _configurationDataDal.Get(x => x.Key == "emailCritical");
            });

            if (emailActive == null)
            {
                emailActive = new ConfigurationData { Key = "emailActive", Value = "False" };
                _configurationDataDal.Add(emailActive);
            }
            if (emailSuccessful == null)
            {
                emailSuccessful = new ConfigurationData { Key = "emailSuccessful", Value = "False" };
                _configurationDataDal.Add(emailSuccessful);
            }
            if (emailFail == null)
            {
                emailFail = new ConfigurationData { Key = "emailFail", Value = "False" };
                _configurationDataDal.Add(emailFail);
            }
            if (emailCritical == null)
            {
                emailCritical = new ConfigurationData { Key = "emailCritical", Value = "False" };
                _configurationDataDal.Add(emailCritical);
            }

            checkEMailNotification.IsChecked = Convert.ToBoolean(emailActive.Value);
            checkEMailNotificationSuccess.IsChecked = Convert.ToBoolean(emailSuccessful.Value);
            checkEMailNotificationFail.IsChecked = Convert.ToBoolean(emailFail.Value);
            checkEMailNotificationCritical.IsChecked = Convert.ToBoolean(emailCritical.Value);

        }

        #endregion


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
            _logger.Verbose("btnCreateTask_Click butonuna istekte bulunuldu");

            List<BackupStorageInfo> backupStorageInfoList = new List<BackupStorageInfo>();
            foreach (BackupStorageInfo item in listViewBackupStorage.Items)
            {
                backupStorageInfoList.Add(item);
            }

            List<TaskInfo> overlappingTaskInfoList = new List<TaskInfo>();
            List<VolumeInfo> volumeInfoList = new List<VolumeInfo>();
           
            foreach (VolumeInfo item in listViewDisk.SelectedItems)
            {
                volumeInfoList.Add(item);

                string letter = item.Letter.ToString();
                var taskInfo = _taskInfoDal.Get(x => x.StrObje.Contains(letter) && x.Type == TaskType.Backup && x.EnableDisable != TecnicalTaskStatusType.Broken);
                if (taskInfo != null)
                {
                    if (overlappingTaskInfoList.Count == 0)
                    {
                        overlappingTaskInfoList.Add(taskInfo);
                    }
                    else
                    {
                        bool controlFlag = false;
                        foreach (var itemTask in overlappingTaskInfoList)
                        {
                            if (itemTask.Id == taskInfo.Id)
                            {
                                controlFlag = false;
                                break;
                            }
                            controlFlag = true;
                        }
                        if (controlFlag)
                            overlappingTaskInfoList.Add(taskInfo);
                    }
                }
            }

            if (overlappingTaskInfoList.Count >= 1)
            {
                string message = "";
                foreach (var itemTask in overlappingTaskInfoList)
                {
                    message += "\n" + itemTask.Name;
                }
                MessageBoxResult result = MessageBox.Show(Resources["taskBackupOverlappingAffectedMB"].ToString() + message, Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
                if (result == MessageBoxResult.Yes)
                { 
                    overlappingTaskInfoList.ForEach(x => BreakTheTask(x)); // sil ve devam et
                }
                else
                {
                    return;
                }
            }

            using (var scope = _scope.BeginLifetimeScope())
            {
                NewCreateTaskWindow newCreateTask = scope.Resolve<NewCreateTaskWindow>(new TypedParameter(backupStorageInfoList.GetType(), backupStorageInfoList),
                    new TypedParameter(volumeInfoList.GetType(), volumeInfoList));
                newCreateTask.ShowDialog();
                if (newCreateTask._showTaskTab)
                    mainTabControl.SelectedIndex = 1;
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
            if (_diskList.Count == _diskExpanderIndex)
                _diskExpanderIndex = 0;
            size.Text = FormatBytes(_diskList[_diskExpanderIndex++].Size);
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

        private void btnTaskRefreshDisk_Click(object sender, RoutedEventArgs e)
        {
            RefreshDisk();
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


        #region Disk Clone

        private void btnDiskCloneStart_Click(object sender, RoutedEventArgs e)
        {
            List<VolumeInfo> sourceVolumes = new List<VolumeInfo>();
            List<VolumeInfo> targetVolumes = new List<VolumeInfo>();

            foreach (VolumeInfo item in listViewDiskCloneSource.SelectedItems)
                sourceVolumes.Add(item);

            foreach (VolumeInfo item in listViewDiskCloneTarget.SelectedItems)
                targetVolumes.Add(item);

            bool sameVolume = false;
            foreach (var sourceVolume in sourceVolumes)
            {
                foreach (var targetVolume in targetVolumes)
                {
                    if (sourceVolume.Letter == targetVolume.Letter)
                    {
                        sameVolume = true;
                        break;
                    }
                }
            }
            if (sameVolume)
            {
                MessageBox.Show(Resources["Aynı dizinde klonlama yapılamaz!"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void HeaderDiskCloneSourceCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            var headerCheckBox = sender as CheckBox;
            _diskCloneSourceDiskAllControl = true;

            foreach (VolumeInfo item in listViewDiskCloneSource.Items)
            {
                if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewDiskCloneSource.SelectedItems.Add(item);
                }
            }
        }

        private void HeaderDiskCloneSourceCheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_diskCloneSourceDiskAllControl)
            {
                var headerCheckBox = sender as CheckBox;
                foreach (VolumeInfo item in listViewDiskCloneSource.Items)
                {
                    if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                    {
                        listViewDiskCloneSource.SelectedItems.Remove(item);
                    }
                }
                _diskAllHeaderControl = true;
            }
        }
        private void HeaderDiskCloneTargetCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            var headerCheckBox = sender as CheckBox;
            _diskCloneTargetDiskAllControl = true;

            foreach (VolumeInfo item in listViewDiskCloneTarget.Items)
            {
                if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewDiskCloneTarget.SelectedItems.Add(item);
                }
            }
        }

        private void HeaderDiskCloneTargetCheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_diskCloneTargetDiskAllControl)
            {
                var headerCheckBox = sender as CheckBox;
                foreach (VolumeInfo item in listViewDiskCloneTarget.Items)
                {
                    if (item.DiskName.Equals(headerCheckBox.Tag.ToString()))
                    {
                        listViewDiskCloneTarget.SelectedItems.Remove(item);
                    }
                }
                _diskAllHeaderControl = true;
            }
        }

        private void listViewDiskCloneTarget_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewDiskCloneTarget.SelectedIndex != -1)
            {
                if (listViewDiskCloneTarget.SelectedIndex != -1)
                    btnDiskCloneStart.IsEnabled = true;
                //eyüp
                //burada buton enable disable olacak hatayı engellemek için geçici olarak CreateTask'dan kopyaladım
            }
            else
            {
                btnDiskCloneStart.IsEnabled = false;
            }
        }

        private void listViewDiskCloneSource_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewDiskCloneSource.SelectedIndex != -1)
            {
                if (listViewDiskCloneSource.SelectedIndex != -1)
                    btnDiskCloneStart.IsEnabled = true;
                //eyüp
                //burada buton enable disable olacak hatayı engellemek için geçici olarak CreateTask'dan kopyaladım
            }
            else
            {
                btnDiskCloneStart.IsEnabled = false;
            }
        }

        private void ExpanderDiskCloneSource_Loaded(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("ExpanderDiskCloneSource_Loaded istekte bulunuldu");

            var expander = sender as Expander;
            _expanderDiskCloneSourceDiskList.Add(expander);
            var size = expander.FindName("txtDiskCloneSourceTotalSize") as TextBlock;
            var expanderCheck = expander.FindName("HeaderDiskCloneSourceCheckBox") as CheckBox;
            _diskCloneSourceCheckBoxes.Add(expanderCheck);
            var numberOfItems = expander.FindName("txtDiskCloneSourceNumberOfItems") as TextBlock;
            _diskCloneSourceNumberOfItems.Add(Convert.ToInt32(numberOfItems.Text));
            var groupName = expander.FindName("txtDiskCloneSourceGroupName") as TextBlock;
            _diskCloneSourceGroupName.Add(groupName.Text);
            if (_diskList.Count == _diskExpanderIndex)
                _diskExpanderIndex = 0;
            size.Text = FormatBytes(_diskList[_diskExpanderIndex++].Size);
        }

        private void ExpanderDiskCloneTarget_Loaded(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("ExpanderDiskCloneTarget_Loaded istekte bulunuldu");

            var expander = sender as Expander;
            _expanderDiskCloneTargetDiskList.Add(expander);
            var size = expander.FindName("txtDiskCloneTargetTotalSize") as TextBlock;
            var expanderCheck = expander.FindName("HeaderDiskCloneTargetCheckBox") as CheckBox;
            _diskCloneTargetCheckBoxes.Add(expanderCheck);
            var numberOfItems = expander.FindName("txtDiskCloneTargetNumberOfItems") as TextBlock;
            _diskCloneTargetNumberOfItems.Add(Convert.ToInt32(numberOfItems.Text));
            var groupName = expander.FindName("txtDiskCloneTargetGroupName") as TextBlock;
            _diskCloneTargetGroupName.Add(groupName.Text);
            if (_diskList.Count == _diskExpanderIndex)
                _diskExpanderIndex = 0;
            size.Text = FormatBytes(_diskList[_diskExpanderIndex++].Size);
        }

        #endregion


        #region Tasks Tab

        private void btnTaskEdit_Click(object sender, RoutedEventArgs e)
        {
            List<BackupStorageInfo> backupStorageInfoList = new List<BackupStorageInfo>();
            foreach (BackupStorageInfo item in listViewBackupStorage.Items)
            {
                backupStorageInfoList.Add(item);
            }

            TaskInfo taskInfo = (TaskInfo)listViewTasks.SelectedItem;
            //Console.WriteLine("Storage: " + taskInfo.BackupStorageInfoId + /*" - " + taskInfo.BackupStorageInfo.StorageName +*/ "\n" +
            //    "BackupTaskInfı: " + taskInfo.BackupTaskId + /*" - " + taskInfo.BackupTaskInfo.TaskName +*/ "\n" +
            //    "Status: " + taskInfo.StatusInfoId /* + " - " + taskInfo.StatusInfo.TaskName*/);

            using (var scope = _scope.BeginLifetimeScope())
            {
                NewCreateTaskWindow newCreateTask = scope.Resolve<NewCreateTaskWindow>(new TypedParameter(backupStorageInfoList.GetType(), backupStorageInfoList),
                    new TypedParameter(taskInfo.GetType(), taskInfo));
                newCreateTask.ShowDialog();
            }
            GetTasks();
            var _backupStorageService = _scope.Resolve<IBackupStorageService>();
            _logger.Verbose("GetBackupStorages istekte bulunuldu");
            listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, _backupStorageService.BackupStorageInfoList());
        }

        private void btnTaskDelete_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnTaskDelete_Click istekte bulunuldu");

            MessageBoxResult result = MessageBox.Show($" {listViewTasks.SelectedItems.Count} " + Resources["deletePieceMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            if (result == MessageBoxResult.Yes)
            {
                foreach (TaskInfo itemTask in listViewTasks.SelectedItems)
                {
                    // ilgili triggerı sil
                    if (itemTask.ScheduleId != null && !itemTask.ScheduleId.Contains("Now") && itemTask.ScheduleId != "")
                    {
                        var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                        taskSchedulerManager.DeleteJob(itemTask.ScheduleId);
                    }

                    // task tipine göre ilgili task tablosundan bilgisini sil
                    if (itemTask.Type == TaskType.Backup)
                    {
                        _backupTaskDal.Delete(_backupTaskDal.Get(x => x.Id == itemTask.BackupTaskId));
                        // sıfırla
                        if (itemTask.EnableDisable != TecnicalTaskStatusType.Broken)
                        {
                            var backupService = _scope.Resolve<IBackupService>();
                            foreach (var itemLetter in itemTask.StrObje)
                            {
                                try
                                {
                                    backupService.CleanChain(itemLetter);
                                }
                                catch (Exception ex)
                                {
                                    _logger.Error(ex, "Beklenmedik hatadan dolayı {harf} zincir temizleme işlemi gerçekleştirilemedi.", itemLetter);
                                }
                            }
                        }           
                    }
                    else
                    {
                        //restore silme
                        _restoreTaskDal.Delete(_restoreTaskDal.Get(x => x.Id == itemTask.RestoreTaskId));
                    }

                    // ilgili status infosunu sil
                    _statusInfoDal.Delete(_statusInfoDal.Get(x => x.Id == itemTask.StatusInfoId));

                    // kendisini sil
                    _taskInfoDal.Delete(itemTask);
                }
                GetTasks();
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
                StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("taskInfo", taskInfo));
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
                        StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("taskInfo", taskInfo));
                        backupStatus.Show();
                    }
                    else
                    {
                        Console.WriteLine("Backup Inc-Diff başlatılıyor");
                        var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                        if (taskInfo.ScheduleId != null && !taskInfo.ScheduleId.Contains("Now") && taskInfo.ScheduleId != "")
                        {
                            taskSchedulerManager.RunNowTrigger(taskInfo.ScheduleId).Wait();
                        }
                        else
                        {
                            taskSchedulerManager.BackupFullNowJob(taskInfo).Wait();
                        }
                        StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("taskInfo", taskInfo));
                        backupStatus.Show();
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
                    StatusesWindow restoreStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 1), new NamedParameter("taskInfo", taskInfo));
                    restoreStatus.Show();
                }
            }
            else if (taskInfo.Status.Equals(TaskStatusType.Paused))
            {
                try
                {
                    backupService.ResumeTask(taskInfo);
                }
                catch (Exception ex)
                {
                    _logger.Error(ex, "Beklenmedik hatadan dolayı taska devam edilemiyor.");
                    MessageBox.Show(Resources["unexpectedErrorMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            RefreshBackupsandTasks(backupService);
        }

        private void btnTaskPause_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnTaskPause_Click istekte bulunuldu");

            var backupService = _scope.Resolve<IBackupService>();
            try
            {
                backupService.PauseTask((TaskInfo)listViewTasks.SelectedItem);
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Beklenmedik hatadan dolayı task duraklatılamıyor.");
                MessageBox.Show(Resources["unexpectedErrorMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
            RefreshBackupsandTasks(backupService);
        }

        private void btnTaskStop_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnTaskStop_Click istekte bulunuldu");

            var backupService = _scope.Resolve<IBackupService>();
            try
            {
                var task = (TaskInfo)listViewTasks.SelectedItem;
                if (task.Type == TaskType.Restore) // Restore stop edilirse disk veya volume bozulmuş olacağından aynı görev başlatılamaz
                    BreakTheTask(task);
                backupService.CancelTask(task);
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Beklenmedik hatadan dolayı task durdurulamıyor.");
                MessageBox.Show(Resources["unexpectedErrorMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
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
            task.EnableDisable = TecnicalTaskStatusType.Disable;
            task.LastWorkingDate = Convert.ToDateTime("01/01/0002");

            var backupTask = _backupTaskDal.Get(x => x.Id == task.BackupTaskId);
            backupTask.TaskName = task.Name;
            var resultBackupTask = _backupTaskDal.Add(backupTask);
            if (resultBackupTask == null)
                MessageBox.Show(Resources["cloneFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            else
            {
                var statusInfo = _statusInfoDal.Get(x => x.Id == task.StatusInfoId);
                statusInfo.TaskName = task.Name;
                var resultStatusInfo = _statusInfoDal.Add(statusInfo);
                if (resultStatusInfo == null)
                    MessageBox.Show(Resources["cloneFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
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
                        MessageBox.Show(Resources["cloneFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
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
                returnTask.EnableDisable = TecnicalTaskStatusType.Enable;
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
            }
            listViewTasks.ItemsSource = _taskInfoList;
            DisableTaskButtons();
        }

        private void listViewTasks_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewTasks.SelectedItems.Count > 1)
                ToggleTaskButtons();
            else if (listViewTasks.SelectedItems.Count == 1)
                SingleToggleTaskButtons();
            else
                DisableTaskButtons();
        }

        #region Task Buttons


        private void ToggleTaskButtons()
        {
            _logger.Verbose("ToggleTaskButtons istekte bulunuldu");

            bool runningFlag = false, pauseFlag = false, disableFlag = false, enableFlag = false, restoreTaskFlag = false, brokenTaskFlag = false, readyFlag = false, backupTaskFlag = false;

            foreach (TaskInfo item in listViewTasks.SelectedItems)
            {
                if (item.Type.Equals(TaskType.Restore)) // restore'da düzenleme olmadığı için restore task klonlanamaz
                    restoreTaskFlag = true;
                else
                    backupTaskFlag = true;

                if (item.EnableDisable == TecnicalTaskStatusType.Enable)
                    enableFlag = true;
                else if (item.EnableDisable == TecnicalTaskStatusType.Disable)
                    disableFlag = true;
                else if (item.EnableDisable == TecnicalTaskStatusType.Broken) // geçersiz görev
                    brokenTaskFlag = true;

                if (item.ScheduleId == null || item.ScheduleId == "")
                {
                    enableFlag = true;
                    disableFlag = true;
                }

                if (item.Status.Equals(TaskStatusType.Working))
                    runningFlag = true;
                else if (item.Status.Equals(TaskStatusType.Paused))
                    pauseFlag = true;
                else if (item.Status.Equals(TaskStatusType.Ready) || item.Status.Equals(TaskStatusType.FirstMissionExpected))
                    readyFlag = true;
            }

            DisableTaskButtons();
            // KOPYALAMA, START, PAUSE, STOP, EDİT YOOOOK :)))))))))) xdxd xD

            if (brokenTaskFlag && !runningFlag && !pauseFlag) // çalışmayan ve durdurulmayan tüm görevler silinebilir
            {
                btnTaskDelete.IsEnabled = true;
            }
            else
            {
                if (restoreTaskFlag && backupTaskFlag) // Ortak oldukları tek şey delete
                {
                    // Enable, Disable, silme yapılabilir
                    if (readyFlag && !runningFlag && !pauseFlag)
                    {
                        btnTaskDelete.IsEnabled = true;
                    }
                }
                else if (restoreTaskFlag) // Restore'da disable/Enable yok
                {
                    // restore button durumları
                    if (readyFlag && !runningFlag)
                    {
                        btnTaskDelete.IsEnabled = true;
                    }
                }
                else if (backupTaskFlag) // Hazır, İlk görev bekleniyor, working, hata, durduruldu, iptal edildi
                {
                    // backup button durumları
                    if (disableFlag && !enableFlag)
                    {
                        btnEnableTask.IsEnabled = true;
                        btnTaskDelete.IsEnabled = true; //bunu özellikle kontrol et
                    }
                    else if (enableFlag && !disableFlag)
                    {
                        if (readyFlag && !runningFlag && !pauseFlag)
                        {
                            btnDisableTask.IsEnabled = true;
                            btnTaskDelete.IsEnabled = true;
                        }
                    }
                    else if (enableFlag && disableFlag)
                    {
                        btnEnableTask.IsEnabled = false;
                        btnDisableTask.IsEnabled = false;

                        if (readyFlag && !runningFlag && !pauseFlag)
                        {
                            btnTaskDelete.IsEnabled = true;
                        }
                    }
                }
            }
        }

        private void SingleToggleTaskButtons()
        {
            _logger.Verbose("SingleToggleTaskButtons istekte bulunuldu");

            bool runningFlag = false, pauseFlag = false, disableFlag = false, enableFlag = false, restoreTaskFlag = false, brokenTaskFlag = false, readyFlag = false, backupTaskFlag = false;
            bool ortayaKarisik = false;

            foreach (TaskInfo item in listViewTasks.SelectedItems)
            {
                if (item.Type.Equals(TaskType.Restore)) // restore'da düzenleme olmadığı için restore task klonlanamaz
                    restoreTaskFlag = true;
                else
                    backupTaskFlag = true;

                if (item.EnableDisable == TecnicalTaskStatusType.Enable)
                    enableFlag = true;
                else if (item.EnableDisable == TecnicalTaskStatusType.Disable)
                    disableFlag = true;
                else if (item.EnableDisable == TecnicalTaskStatusType.Broken) // geçersiz görev
                    brokenTaskFlag = true;

                if (item.ScheduleId == null || item.ScheduleId == "")
                {
                    ortayaKarisik = true;
                }

                if (item.Status.Equals(TaskStatusType.Working))
                    runningFlag = true;
                else if (item.Status.Equals(TaskStatusType.Paused))
                    pauseFlag = true;
                else if (item.Status.Equals(TaskStatusType.Ready) || item.Status.Equals(TaskStatusType.FirstMissionExpected))
                    readyFlag = true;
            }

            DisableTaskButtons();

            if (brokenTaskFlag)
            {
                btnTaskDelete.IsEnabled = true;
            }
            else
            {
                if (restoreTaskFlag) // Hazır, İlk görev bekleniyor, working, hata
                {
                    // restore button durumları
                    if (readyFlag) // burada kontrol edilmesi gereken iki buton mevcut delete ve start
                    {
                        btnTaskDelete.IsEnabled = true;
                        btnTaskStart.IsEnabled = true;
                    }
                    else if (runningFlag)
                    {
                        btnTaskPause.IsEnabled = true;
                        btnTaskStop.IsEnabled = true;
                    }
                    else if (pauseFlag)
                    {
                        btnTaskStop.IsEnabled = true;
                        btnTaskStart.IsEnabled = true;
                    }
                }
                else if (backupTaskFlag) // Hazır, İlk görev bekleniyor, working, hata, durduruldu, iptal edildi
                {
                    // backup button durumları
                    if (disableFlag)
                    {
                        btnEnableTask.IsEnabled = true;
                        btnCopyTask.IsEnabled = true;
                        btnTaskEdit.IsEnabled = true;
                        btnTaskDelete.IsEnabled = true;
                    }
                    else if (enableFlag)
                    {
                        btnDisableTask.IsEnabled = true;

                        if (readyFlag)
                        {
                            btnCopyTask.IsEnabled = true;
                            btnTaskEdit.IsEnabled = true;
                            btnTaskDelete.IsEnabled = true;
                            btnTaskStart.IsEnabled = true;
                        }
                        else if (runningFlag)
                        {
                            btnTaskPause.IsEnabled = true;
                            btnTaskStop.IsEnabled = true;
                            btnDisableTask.IsEnabled = false;
                        }
                        else if (pauseFlag)
                        {
                            btnTaskStop.IsEnabled = true;
                            btnTaskStart.IsEnabled = true;
                            btnDisableTask.IsEnabled = false;
                        }
                    }

                    if (ortayaKarisik)
                    {
                        btnEnableTask.IsEnabled = false;
                        btnDisableTask.IsEnabled = false;
                    }
                }
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

        #endregion

        #region Checkbox Operations

        private void chbAllTasksChechbox_Checked(object sender, RoutedEventArgs e)
        {
            _tasksAllControl = true;

            foreach (TaskInfo item in listViewTasks.ItemsSource)
            {
                listViewTasks.SelectedItems.Add(item);
            }

            if (listViewTasks.SelectedItems.Count > 1)
                ToggleTaskButtons();
            else if (listViewTasks.SelectedItems.Count == 1)
                SingleToggleTaskButtons();
            else
                DisableTaskButtons();
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

            if (listViewTasks.SelectedItems.Count > 1)
                ToggleTaskButtons();
            else if (listViewTasks.SelectedItems.Count == 1)
                SingleToggleTaskButtons();
            else
                DisableTaskButtons();
        }

        private void chbTask_Checked(object sender, RoutedEventArgs e)
        {
            if (chbAllTasksChechbox.IsChecked == false)
            {
                _tasksAllControl = listViewTasks.SelectedItems.Count == listViewTasks.Items.Count;
                chbAllTasksChechbox.IsChecked = _tasksAllControl;
            }

            if (listViewTasks.SelectedItems.Count > 1)
                ToggleTaskButtons();
            else if (listViewTasks.SelectedItems.Count == 1)
                SingleToggleTaskButtons();
            else
                DisableTaskButtons();
        }

        private void chbTask_Unchecked(object sender, RoutedEventArgs e)
        {
            _tasksAllControl = false;
            chbAllTasksChechbox.IsChecked = false;

            if (listViewTasks.SelectedItems.Count > 1)
                ToggleTaskButtons();
            else if (listViewTasks.SelectedItems.Count == 1)
                SingleToggleTaskButtons();
            else
                DisableTaskButtons();
        }
        #endregion

        #endregion


        #region Restore Tab

        private void btnRestore_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnRestore_Click istekte bulunuldu");

            List<VolumeInfo> volumeInfoList = new List<VolumeInfo>();

            foreach (VolumeInfo item in listViewRestoreDisk.SelectedItems)
                volumeInfoList.Add(item);

            BackupInfo backupInfo = (BackupInfo)listViewRestore.SelectedItem;
            bool controlFlag = false;

            if (backupInfo.BackupStorageInfo.Type == BackupStorageType.Windows)
            {
                foreach (var item in volumeInfoList)
                {
                    if (item.Letter == backupInfo.BackupStorageInfo.Path[0])
                    {
                        controlFlag = true;
                        MessageBox.Show(Resources["sameRootDirectoryRestoreMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                        break;
                    }
                }
            }

            bool diskFlag = false;
            foreach (var item in _restoreExpanderCheckBoxes)
            {
                if (item.IsChecked.Value)
                {
                    diskFlag = true;
                    break;
                }
            }

            if (diskFlag && !controlFlag)
            {
                //disk kontrol et
                foreach (var item in _diskList)
                {
                    if (item.VolumeInfos[0].DiskName.Equals(volumeInfoList[0].DiskName))
                    {
                        if (backupInfo.VolumeSize > item.Size)
                        {
                            controlFlag = true;
                            MessageBox.Show(Resources["sizeConflictMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                        }
                    }
                }
            }
            else
            {
                if (backupInfo.VolumeSize > volumeInfoList[0].Size)
                {
                    controlFlag = true;
                    MessageBox.Show(Resources["sizeConflictMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }

            if (!controlFlag)
            {
                using (var scope = _scope.BeginLifetimeScope())
                {
                    RestoreWindow restore = scope.Resolve<RestoreWindow>(new TypedParameter(backupInfo.GetType(), backupInfo),
                        new TypedParameter(volumeInfoList.GetType(), volumeInfoList), new TypedParameter(diskFlag.GetType(), diskFlag));
                    restore.ShowDialog();
                    if (restore._showTaskTab)
                        mainTabControl.SelectedIndex = 1;
                }
            }
            GetTasks();
        }

        private void ExpanderRestore_Loaded(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("ExpanderRestore_Loaded istekte bulunuldu");

            var expander = sender as Expander;
            _expanderRestoreDiskList.Add(expander);
            var size = expander.FindName("txtRestoreTotalSize") as TextBlock;
            var expanderCheck = expander.FindName("cbRestoreHeader") as CheckBox;
            _restoreExpanderCheckBoxes.Add(expanderCheck);
            var numberOfItems = expander.FindName("txtRestoreNumberOfItems") as TextBlock;
            _restoreNumberOfItems.Add(Convert.ToInt32(numberOfItems.Text));
            var groupName = expander.FindName("txtRestoreGroupName") as TextBlock;
            _restoreGroupName.Add(groupName.Text);
            if (_diskList.Count == _diskExpanderIndex)
                _diskExpanderIndex = 0;
            size.Text = FormatBytes(_diskList[_diskExpanderIndex++].Size);
        }

        private void listViewRestore_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewRestore.SelectedIndex != -1)
            {
                if (listViewRestoreDisk.SelectedIndex != -1)
                    btnRestore.IsEnabled = true;
                /*BAKILACAK*/
                BackupInfo backupInfo = (BackupInfo)listViewRestore.SelectedItem;

                txtRDriveLetter.Text = backupInfo.Letter.ToString();
                txtROS.Text = backupInfo.OS;
                if (backupInfo.DiskType.Equals('M'))
                    txtRBootPartition.Text = "MBR";
                else
                    txtRBootPartition.Text = "GPT";

                txtRVolumeSize.Text = backupInfo.StrVolumeSize;
                txtRUsedSize.Text = backupInfo.StrUsedSize;

                txtRDriveLetter.Text = backupInfo.Letter.ToString();
                txtRCreatedDate.Text = backupInfo.CreatedDate.ToString();
                txtRPcName.Text = backupInfo.PCName;
                txtRIpAddress.Text = backupInfo.IpAddress;
                txtRFolderSize.Text = backupInfo.StrFileSize;
                if (Convert.ToBoolean(backupInfo.OSVolume))
                    txtRBootablePartition.Text = Resources["yes"].ToString();
                else
                    txtRBootablePartition.Text = Resources["no"].ToString();
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

        private void btnRestoreRefreshDisk_Click(object sender, RoutedEventArgs e)
        {
            RefreshDisk();
        }

        #region Checkbox Operations

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
            else
            {
                btnFilesDelete.IsEnabled = false;
                btnFilesBrowse.IsEnabled = false;
            }
        }

        private void btnRefreshBackups_Click(object sender, RoutedEventArgs e)
        {
            var backupService = _scope.Resolve<IBackupService>();
            RefreshBackups(backupService);
        }

        private void btnFilesDelete_Click(object sender, RoutedEventArgs e)
        {
            var backupService = _scope.Resolve<IBackupService>();
            //var backupInfo = (BackupInfo)listViewBackups.SelectedItem;
            MessageBoxResult result = MessageBoxResult.No;
            if (listViewBackups.SelectedItems.Count >= 1)
            {
                bool controlFlag = false;
                foreach (BackupInfo backupInfo in listViewBackups.SelectedItems)
                {
                    if (backupInfo.Type == BackupTypes.Full)
                    {
                        result = MessageBox.Show(Resources["fullBackupDeleteMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
                        controlFlag = true;
                        break;
                    }
                }
                if (!controlFlag)
                    result = MessageBox.Show($" {listViewBackups.SelectedItems.Count} " + Resources["deleteBackupPieceMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            }
            else
                result = MessageBox.Show($"{((BackupInfo)listViewBackups.SelectedItem).FileName} " + Resources["deleteBackupPieceMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);

            if (result == MessageBoxResult.Yes)
            {
                Dictionary<string, bool> NAS = new Dictionary<string, bool>();
                Dictionary<string, bool> tempNas = new Dictionary<string, bool>();
                NAS.Add("EbruAndEyup", true);

                foreach (BackupInfo backupInfo in listViewBackups.SelectedItems)
                {
                    if (backupInfo.BackupStorageInfo.Type == BackupStorageType.NAS)
                    {
                        bool controlFlag = false, changeFlag = false;
                        foreach (var item in NAS)
                        {
                            if (backupInfo.BackupStorageInfo.Path.Contains(item.Key))
                            {
                                if (!item.Value)
                                {
                                    using (var scope = _scope.BeginLifetimeScope())
                                    {
                                        ValidateNASWindow validateNASWindow = scope.Resolve<ValidateNASWindow>(new TypedParameter(backupInfo.GetType(), backupInfo));
                                        validateNASWindow.ShowDialog();
                                        var ip = backupInfo.BackupStorageInfo.Path.Split('\\')[2];
                                        tempNas.Add(item.Key, validateNASWindow._validate);
                                        controlFlag = true;
                                        changeFlag = true;
                                    }
                                }
                                else if (item.Value)
                                {
                                    try
                                    {
                                        var result2 = backupService.BackupFileDelete(backupInfo);
                                        if (result2 == 0)
                                            MessageBox.Show(Resources["notConnectNASMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                                        else if (result2 == 1)
                                            MessageBox.Show(Resources["deleteFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                                    }
                                    catch (Exception ex)
                                    {
                                        _logger.Error(ex, "Beklenmedik hatadan dolayı silme işlemi gerçekleştirilemedi.");
                                        MessageBox.Show(Resources["deleteFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                                    }
                                    controlFlag = true;
                                }
                            }
                        }

                        if (changeFlag)
                        {
                            NAS[tempNas.Keys.First()] = tempNas.Values.First();
                            tempNas.Clear();
                        }

                        if (!controlFlag)
                        {
                            using (var scope = _scope.BeginLifetimeScope())
                            {
                                ValidateNASWindow validateNASWindow = scope.Resolve<ValidateNASWindow>(new TypedParameter(backupInfo.GetType(), backupInfo));
                                validateNASWindow.ShowDialog();
                                var ip = backupInfo.BackupStorageInfo.Path.Split('\\')[2];
                                NAS.Add(ip, validateNASWindow._validate);
                            }
                        }
                    }
                    else if (backupInfo.BackupStorageInfo.Type == BackupStorageType.Windows)
                    {
                        // silme işlemleri 
                        try
                        {
                            var result2 = backupService.BackupFileDelete(backupInfo);
                            if (result2 == 0)
                                MessageBox.Show(Resources["notConnectNASMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                            else if (result2 == 1)
                                MessageBox.Show(Resources["deleteFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                        }
                        catch (Exception ex)
                        {
                            _logger.Error(ex, "Beklenmedik hatadan dolayı silme işlemi gerçekleştirilemedi.");
                            MessageBox.Show(Resources["deleteFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                        }
                    }
                }
            }

            RefreshBackups(backupService);
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

        public List<BackupStorageInfo> GetBackupStorages(List<VolumeInfo> volumeList, List<BackupStorageInfo> backupStorageInfoList) //eyüp
        {
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
                if (storageItem.Type == BackupStorageType.NAS) //Nas boyut bilgisi hesaplama
                {
                    var _backupStorageService = _scope.Resolve<IBackupStorageService>();
                    var nasConnection = _backupStorageService.GetNasCapacityAndSize((storageItem.Path.Substring(0, storageItem.Path.Length - 1)), storageItem.Username, storageItem.Password, storageItem.Domain);
                    storageItem.Capacity = Convert.ToInt64(nasConnection[0]);
                    storageItem.UsedSize = Convert.ToInt64(nasConnection[0]) - Convert.ToInt64(nasConnection[1]);
                    storageItem.FreeSize = Convert.ToInt64(nasConnection[1]);
                    storageItem.StrCapacity = FormatBytes(Convert.ToInt64(nasConnection[0]));
                    storageItem.StrUsedSize = FormatBytes(Convert.ToInt64(nasConnection[0]) - Convert.ToInt64(nasConnection[1]));
                    storageItem.StrFreeSize = FormatBytes(Convert.ToInt64(nasConnection[1]));
                }
            }

            return backupStorageInfoList;
        }

        private void btnRefreshBackupAreas_Click(object sender, RoutedEventArgs e)
        {
            var backupStorageService = _scope.Resolve<IBackupStorageService>();
            listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, backupStorageService.BackupStorageInfoList());
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
                var backupService = _scope.Resolve<IBackupService>();
                RefreshBackupsandTasks(backupService);
            }
        }

        private void btnBackupStorageEdit_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnBackupStorageEdit_Click istekte bulunuldu");

            bool taskRunnigFlag = false;
            foreach (TaskInfo itemTask in listViewTasks.Items)
            {
                if (itemTask.BackupStorageInfoId == ((BackupStorageInfo)listViewBackupStorage.SelectedItem).Id && (itemTask.Status == TaskStatusType.Error || itemTask.Status == TaskStatusType.Paused || itemTask.Status == TaskStatusType.Working))
                    taskRunnigFlag = true;
            }
            if (taskRunnigFlag)
            {
                MessageBox.Show(Resources["runningTaskAffectedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }

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
                var backupService = _scope.Resolve<IBackupService>();
                RefreshBackupsandTasks(backupService);
            }
        }

        private void btnBackupStorageDelete_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnBackupStorageDelete_Click istekte bulunuldu");

            // etkilenen task kontrolü
            MessageBoxResult result;
            bool taskReadyFlag = false, taskRunnigFlag = false;
            foreach (TaskInfo itemTask in listViewTasks.Items)
            {
                foreach (BackupStorageInfo item in listViewBackupStorage.SelectedItems)
                {
                    if (itemTask.BackupStorageInfoId == item.Id && (itemTask.Status == TaskStatusType.Ready || itemTask.Status == TaskStatusType.FirstMissionExpected || itemTask.Status == TaskStatusType.Cancel))
                    {
                        taskReadyFlag = true;
                    }
                    else if (itemTask.BackupStorageInfoId == item.Id)
                    {
                        taskRunnigFlag = true;
                    }
                }
            }

            if (taskReadyFlag && !taskRunnigFlag)
                result = MessageBox.Show($"{listViewBackupStorage.SelectedItems.Count} " + Resources["deleteTaskPieceMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            else if (taskRunnigFlag)
            {
                MessageBox.Show(Resources["runningTaskAffectedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }
            else
                result = MessageBox.Show($"{listViewBackupStorage.SelectedItems.Count} " + Resources["deletePieceMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);

            if (result == MessageBoxResult.Yes)
            {
                var backupStorageService = _scope.Resolve<IBackupStorageService>();
                foreach (BackupStorageInfo item in listViewBackupStorage.SelectedItems)
                {
                    backupStorageService.DeleteBackupStorage(item);
                }

                foreach (TaskInfo itemTask in listViewTasks.Items) //etkilenen taskları etkisizleştir
                {
                    if (_backupStorageDal.Get(x => x.Id == itemTask.BackupStorageInfoId) == null)
                    {
                        BreakTheTask(itemTask);
                    }
                }

                _logger.Verbose("GetBackupStorages istekte bulunuldu");
                listViewBackupStorage.ItemsSource = GetBackupStorages(_volumeList, backupStorageService.BackupStorageInfoList());
                var backupService = _scope.Resolve<IBackupService>();
                RefreshBackupsandTasks(backupService);
            }
        }

        private void BreakTheTask(TaskInfo itemTask)
        {
            itemTask.EnableDisable = TecnicalTaskStatusType.Broken;
            itemTask.BackupStorageInfoId = 0;

            if (itemTask.ScheduleId != null && itemTask.ScheduleId != "")
            {
                var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                taskSchedulerManager.DeleteJob(itemTask.ScheduleId);
                itemTask.ScheduleId = "";
            }

            if (itemTask.Type == TaskType.Backup) //clean chain yaparak zincir sıfırlanmalı
            {
                var backupService = _scope.Resolve<IBackupService>();
                foreach (var itemLetter in itemTask.StrObje)
                {
                    try
                    {
                        backupService.CleanChain(itemLetter);
                    }
                    catch (Exception ex)
                    {
                        _logger.Error(ex, "Beklenmedik hatadan dolayı {harf} zincir temizleme işlemi gerçekleştirilemedi.", itemLetter);
                    }
                }
            }

            _taskInfoDal.Update(itemTask);
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
                StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("activityLog", activityLog));
                backupStatus.Show();
            }
        }

        private void btnLogDelete_Click(object sender, RoutedEventArgs e)
        {
            _logger.Verbose("btnLogDelete_Click istekte bulunuldu");

            MessageBoxResult result = MessageBox.Show($"{listViewLog.SelectedItems.Count} " + Resources["deletePieceMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
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
                item.StatusInfo = _statusInfoDal.Get(x => x.Id == item.StatusInfoId);
                item.StatusInfo.StrStatus = Resources[$"{item.StatusInfo.StrStatus}"].ToString();
            }

            listViewLog.ItemsSource = _activityLogList;
            CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewLog.ItemsSource);
            view.SortDescriptions.Add(new SortDescription("Id", ListSortDirection.Descending));
        }

        #endregion


        #region Settings Tab

        private void cbLang_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (cbLang.SelectedIndex != -1 && cbLang.SelectedItem is KeyValuePair<string, string> item)
            {
                var languageConfiguration = _configurationDataDal.Get(x => x.Key == "lang");
                if (item.Value != languageConfiguration.Value)
                {
                    languageConfiguration.Value = item.Value;
                    _configurationDataDal.Update(languageConfiguration);
                    SetApplicationLanguage(item.Value);
                    ReloadLanguages();
                    cbLang.SelectedValue = item.Value;

                    //aktivity log güncelleniyor çünkü statuler önceki dil ayarı olarak kalıyordu
                    ShowActivityLog();
                }
            }
        }

        private void ReloadLanguages()
        {
            languages.Clear();
            languages[Resources["english"].ToString()] = "en";
            languages[Resources["turkish"].ToString()] = "tr";
            cbLang.ItemsSource = null;
            cbLang.Items.Clear();
            cbLang.ItemsSource = languages;
            txtLicenseStatu.Text = Resources[ReloadLicenseStatu(txtLicenseStatu.Text)].ToString();
            txtMachineType.Text = Resources[ReoladMachineType(txtMachineType.Text)].ToString();
        }

        private string ReloadLicenseStatu(string activeStatus)
        {
            if (activeStatus.Equals("İnactive") || activeStatus.Equals("Aktif Değil"))
            {
                return "inactive";
            }
            else if (activeStatus.Equals("Demo"))
            {
                return "demo";
            }
            else
            {
                return "active";
            }
        }

        private string ReoladMachineType(string activeStatus)
        {
            if (activeStatus.Equals("Virtual Machine") || activeStatus.Equals("Sanal Makine"))
            {
                return "VirtualMachine";
            }
            else
            {
                return "PhysicalMachine";
            }
        }

        private void btnValidateLicense_Click(object sender, RoutedEventArgs e)
        {
            var resultLicense = _licenseService.ValidateLicenseKey(txtLicenseKey.Text);
            ValidateLicenseKeyController(resultLicense);
        }

        private void btnAddLicenseFile_Click(object sender, RoutedEventArgs e)
        {
            using (var dialog = new System.Windows.Forms.OpenFileDialog())
            {
                dialog.Filter = "Narbulut Key Dosyası |*.nbkey";
                dialog.ValidateNames = false;
                dialog.CheckFileExists = false;
                dialog.CheckPathExists = true;
                dialog.Multiselect = false;

                var result = dialog.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    string nbkeyPath = dialog.FileName;
                    StreamReader sr = new StreamReader(nbkeyPath);
                    var resultLicense = _licenseService.ValidateLicenseKey(sr.ReadToEnd());
                    ValidateLicenseKeyController(resultLicense);
                }
            }
        }

        private void ValidateLicenseKeyController(string resultLicense)
        {
            if (resultLicense.Equals("fail"))
            {
                MessageBox.Show(Resources["LicenseKeyFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else if (resultLicense.Equals("failMachine"))
            {
                MessageBox.Show(Resources["LicenseKeyMachineFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else if (resultLicense.Equals("failOS"))
            {
                MessageBox.Show(Resources["LicenseKeyOSFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else
            {
                // lisans key etkinleştirildi
                _logger.Information("Lisans aktifleştirildi. Lisans Anahtarı: " + txtLicenseKey.Text);
                stackDemo.Visibility = Visibility.Collapsed;
                txtLicenseKey.Text = "";
                LicenseInformationWrite(_licenseService.GetLicenseUserInfo());
            }
        }


        #region E-Mail Ayarlar kutusu

        private void checkEMailNotification_Checked(object sender, RoutedEventArgs e)
        {
            stackEMailNotification.IsEnabled = true;
        }

        private void checkEMailNotification_Unchecked(object sender, RoutedEventArgs e)
        {
            stackEMailNotification.IsEnabled = false;
            checkEMailNotificationFail.IsChecked = false;
            checkEMailNotificationSuccess.IsChecked = false;
            checkEMailNotificationCritical.IsChecked = false;
        }

        private void btnEMailAdvancedOptions_Click(object sender, RoutedEventArgs e)
        {
            using (var scope = _scope.BeginLifetimeScope())
            {
                EMailSettingsWindow emailSettingsWindow = scope.Resolve<EMailSettingsWindow>();
                emailSettingsWindow.ShowDialog();
            }
        }

        private void btnEMailNotificationSave_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var emailActive = _configurationDataDal.Get(x => x.Key == "emailActive");
                emailActive.Value = checkEMailNotification.IsChecked.ToString();
                _configurationDataDal.Update(emailActive);

                var emailSuccessful = _configurationDataDal.Get(x => x.Key == "emailSuccessful");
                emailSuccessful.Value = checkEMailNotificationSuccess.IsChecked.ToString();
                _configurationDataDal.Update(emailSuccessful);

                var emailFail = _configurationDataDal.Get(x => x.Key == "emailFail");
                emailFail.Value = checkEMailNotificationFail.IsChecked.ToString();
                _configurationDataDal.Update(emailFail);

                var emailCritical = _configurationDataDal.Get(x => x.Key == "emailCritical");
                emailCritical.Value = checkEMailNotificationCritical.IsChecked.ToString();
                _configurationDataDal.Update(emailCritical);

                MessageBox.Show(Resources["notificationUpdateMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                _logger.Error("Notification ayarları değiştirilemedi. " + ex);
                MessageBox.Show(Resources["notificationUpdateFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }

        }

        #endregion

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

        private void btnSendFeedBack_Click(object sender, RoutedEventArgs e)
        {
            var mailOperations = _scope.Resolve<IEMailOperations>();
            string feedbackType = "";
            if (cbFeedbackMessageType.SelectedIndex == 0)
                feedbackType = "Öneri";
            else if (cbFeedbackMessageType.SelectedIndex == 1)
                feedbackType = "Hata Bildirimi";
            else if (cbFeedbackMessageType.SelectedIndex == 2)
                feedbackType = "Şikayet";

            bool mailSendOperaitonResponse = mailOperations.SendFeedback(feedbackTxt.Text, feedbackType, _licenseService.GetLicenseUserInfo());

            if (mailSendOperaitonResponse)
            {
                MessageBox.Show("Mail gönderimi başarılı");
                feedbackTxt.Text = "";
            }
            else
                MessageBox.Show("Mail gönderilemedi");
        }

        #endregion


        #region Refresh function

        public async void RefreshDemoDays(CancellationToken cancellationToken)
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    await Task.Delay(60000, cancellationToken);
                    _logger.Verbose("RefreshDemoDays istekte bulunuldu");

                    LicenseController();
                }
                catch (Exception e)
                {
                    if (!cancellationToken.IsCancellationRequested) // lisans doğrulama yapılmazsa cancel ediliyor, refresh çalıştığından hata veriyordu onun kontrolü
                    {
                        //if (e.Message.Contains("System.ServiceModel.Channels.ServiceChannel")) // TO DO servis kapanma kontrolü yapılırsa hoş olur
                        //{
                        MessageBox.Show(Resources["closeServiceErrorMB"].ToString() + "\n" + e.ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error, MessageBoxResult.No);
                        Close();
                        return;
                        //}
                    }
                }

            }
        }

        public async void RefreshTasks(CancellationToken cancellationToken, IBackupService backupService)
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    await Task.Delay(500, cancellationToken);
                    _logger.Verbose("RefreshTasks istekte bulunuldu");
                    //var backupService = _scope.Resolve<IBackupService>();

                    //log down
                    RefreshActivityLogDownAsync(backupService);

                    // son yedekleme bilgisi
                    RefreshMiddleTaskDateAsync();

                    // Ortadaki statu
                    RefreshMiddleTaskStatuAsync();

                    // TO DO --- ASYNC DEGİLLER BAKILACAK
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
                    if (!cancellationToken.IsCancellationRequested) // lisans doğrulama yapılmazsa cancel ediliyor, refresh çalıştığından hata veriyordu onun kontrolü
                    {
                        //if (e.Message.Contains("System.ServiceModel.Channels.ServiceChannel")) // TO DO servis kapanma kontrolü yapılırsa hoş olur
                        //{
                        MessageBox.Show(Resources["closeServiceErrorMB"].ToString() + "\n" + e.ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error, MessageBoxResult.No);
                        Close();
                        return;
                        //}
                    }
                }

            }
        }

        private async void RefreshActivityLogDownAsync(IBackupService backupService)
        {
            _logger.Verbose("RefreshActivityLogDownAsync istekte bulunuldu");

            List<ActivityDownLog> logList = new List<ActivityDownLog>();
            try
            {
                logList = await Task.Run(() =>
                {
                    return backupService.GetDownLogList();
                });
            }
            catch
            {
            }

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

        private async void RefreshMiddleTaskDateAsync()
        {
            _logger.Verbose("RefreshMiddleTaskDateAsync istekte bulunuldu");

            if (listViewLog.Items.Count > 0)
            {
                _logger.Verbose("RefreshTasks: Son başarılı-başarısız tarih yazdırılıyor");

                var lastLog = await Task.Run(() =>
                {
                    return _activityLogDal.GetList().LastOrDefault();
                });
                lastLog.StatusInfo = _statusInfoDal.Get(x => x.Id == lastLog.StatusInfoId);
                //ActivityLog lastLog = ((ActivityLog)listViewLog.Items[0]);
                txtRunningStateBlock.Text = lastLog.EndDate.ToString();
                if (lastLog.StatusInfo.Status == StatusType.Success)
                    txtRunningStateBlock.Foreground = Brushes.Green;
                else
                    txtRunningStateBlock.Foreground = Brushes.Red;
            }
        }

        private async void RefreshMiddleTaskStatuAsync()
        {
            _logger.Verbose("RefreshMiddleTaskStatuAsync istekte bulunuldu");

            TaskInfo runningTask = await Task.Run(() =>
            {
                return _taskInfoDal.GetList(x => x.Status == TaskStatusType.Working).FirstOrDefault();
            });

            TaskInfo pausedTask = await Task.Run(() =>
            {
                return _taskInfoDal.GetList(x => x.Status == TaskStatusType.Paused).FirstOrDefault();
            });

            if (runningTask != null)
            {
                _logger.Verbose("RefreshTasks: Çalışan task yazdırılıyor");

                // çalışanı yazdır
                runningTask.StatusInfo = _statusInfoDal.Get(x => x.Id == runningTask.StatusInfoId);
                if (runningTask.Type == TaskType.Backup)
                {
                    txtMakeABackup.Text = Resources["makeABackup"].ToString() + ", " + FormatBytes(runningTask.StatusInfo.DataProcessed);
                }
                else
                {
                    txtMakeABackup.Text = Resources["backupRestore"].ToString() + ", " + FormatBytes(runningTask.StatusInfo.DataProcessed);
                }
                if (double.IsNaN(Math.Round((runningTask.StatusInfo.DataProcessed * 100.0) / (runningTask.StatusInfo.TotalDataProcessed), 2)))
                    txtMakeABackup.Text += ", %0"; //TO DO NaN yakalandığında buraya 0 dışında bir şey girilmek istenir mi?
                else
                    txtMakeABackup.Text += ", %" + Math.Round((runningTask.StatusInfo.DataProcessed * 100.0) / (runningTask.StatusInfo.TotalDataProcessed), 2).ToString();


            }
            else if (pausedTask != null)
            {
                _logger.Verbose("RefreshTasks: Durdurulan task yazdırılıyor");

                // durdurulanı yazdır
                pausedTask.StatusInfo = _statusInfoDal.Get(x => x.Id == pausedTask.StatusInfoId);
                txtMakeABackup.Text = Resources["backupStopped"].ToString() + ", "
                    + FormatBytes(pausedTask.StatusInfo.DataProcessed)
                    + ", %" + Math.Round((pausedTask.StatusInfo.DataProcessed * 100.0) / (pausedTask.StatusInfo.TotalDataProcessed), 2).ToString();
            }
            else
                txtMakeABackup.Text = "";
        }

        private void RefreshBackupsandTasks(IBackupService backupService)
        {
            _logger.Verbose("RefreshBackupsandTasks istekte bulunuldu");

            int taskSelectedIndex = -1;
            if (listViewTasks.SelectedIndex != -1)
            {
                taskSelectedIndex = listViewTasks.SelectedIndex;
            }
            GetTasks();
            listViewTasks.SelectedIndex = taskSelectedIndex;

            RefreshBackups(backupService);
        }

        private void RefreshBackups(IBackupService backupService)
        {
            _logger.Verbose("RefreshBackups istekte bulunuldu");
            try
            {
                _backupsItems = backupService.GetBackupFileList(_backupStorageDal.GetList());
                listViewBackups.ItemsSource = _backupsItems;
                listViewRestore.ItemsSource = _backupsItems;
            }
            catch (Exception ex)
            {
                _logger.Error("RefreshBackups methodu çağırılırken hata oluştu. Hata:" + ex.Message);
            }
        }

        private void RefreshActivityLog(IBackupService backupService)
        {
            _logger.Verbose("RefreshActivityLog istekte bulunuldu");

            try
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
            catch
            {
            }
        }

        private void GetDiskPage()
        {
            _logger.Verbose("GetDiskPage istekte bulunuldu, Disk Pro.Bar'ları güncelleniyor");
            diskInfoStackPanel.Children.Clear();
            stackTasksDiskInfo.Children.Clear();

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
        }

        private void RefreshDisk()
        {
            _logger.Verbose("RefreshDisk istekte bulunuldu, Disk Listviewler güncelleniyor");
            try
            {
                _expanderCheckBoxes.Clear();
                _numberOfItems.Clear();
                _groupName.Clear();
                _restoreExpanderCheckBoxes.Clear();
                _restoreNumberOfItems.Clear();
                _restoreGroupName.Clear();
                _expanderRestoreDiskList.Clear();

                var backupService = _scope.Resolve<IBackupService>();
                _diskList = backupService.GetDiskList();
                _volumeList.Clear();

                foreach (var diskItem in _diskList)
                {
                    foreach (var volumeItem in diskItem.VolumeInfos)
                    {
                        if (volumeItem.HealthStatu == HealthSituation.Healthy)
                            volumeItem.Status = Resources["healthy"].ToString();
                        else
                            volumeItem.Status = Resources["unhealthy"].ToString();

                        _volumeList.Add(volumeItem);
                    }
                }
                //_volumeList.ForEach(x => Console.WriteLine(x.Name));
                _view.Refresh();
            }
            catch (Exception ex)
            {
                _logger.Fatal(ex, "Disk listesi yenilenemedi.");
                MessageBox.Show(Resources["unexpectedError1MB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                Close();
            }

        }

        #endregion


        #region Genel Fonksiyonlar

        private void mainTabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (e.Source != mainTabControl)
                return;
            if (mainTabControl.SelectedIndex == 0 || mainTabControl.SelectedIndex == 2)
                RefreshDisk();
            if (mainTabControl.SelectedIndex == 0 || mainTabControl.SelectedIndex == 1)
                GetDiskPage();
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

        #endregion


        #region Listviews Sort

        //ActivityLog
        private GridViewColumnHeader listViewLogCol = null;
        private SortAdorner listViewLogAdorner = null;

        private void listviewLogColumnHeaderSort_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewLogCol != null)
            {
                AdornerLayer.GetAdornerLayer(listViewLogCol).Remove(listViewLogAdorner);
                listViewLog.Items.SortDescriptions.Clear();
            }
            else
            {
                listViewLog.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewLogCol == column && listViewLogAdorner.Direction == newDir)
            {
                newDir = ListSortDirection.Descending;
            }

            listViewLogCol = column;
            listViewLogAdorner = new SortAdorner(listViewLogCol, newDir);
            AdornerLayer.GetAdornerLayer(listViewLogCol).Add(listViewLogAdorner);
            listViewLog.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));
        }

        //Backup Storage
        private GridViewColumnHeader listViewBackupStorageCol = null;
        private SortAdorner listViewBackupStorageAdorner = null;

        private void listviewBackupStorageColumnHeaderSort_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewBackupStorageCol != null)
            {
                AdornerLayer.GetAdornerLayer(listViewBackupStorageCol).Remove(listViewBackupStorageAdorner);
                listViewBackupStorage.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewBackupStorageCol == column && listViewBackupStorageAdorner.Direction == newDir)
                newDir = ListSortDirection.Descending;

            listViewBackupStorageCol = column;
            listViewBackupStorageAdorner = new SortAdorner(listViewBackupStorageCol, newDir);
            AdornerLayer.GetAdornerLayer(listViewBackupStorageCol).Add(listViewBackupStorageAdorner);
            listViewBackupStorage.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));
        }

        //Yedekleri görüntüle
        private GridViewColumnHeader listViewBackupsCol = null;
        private SortAdorner listViewBackupsAdorner = null;

        private void listviewBackupsColumnHeaderSort_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewBackupsCol != null)
            {
                AdornerLayer.GetAdornerLayer(listViewBackupsCol).Remove(listViewBackupsAdorner);
                listViewBackups.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewBackupsCol == column && listViewBackupsAdorner.Direction == newDir)
                newDir = ListSortDirection.Descending;

            listViewBackupsCol = column;
            listViewBackupsAdorner = new SortAdorner(listViewBackupsCol, newDir);
            AdornerLayer.GetAdornerLayer(listViewBackupsCol).Add(listViewBackupsAdorner);
            listViewBackups.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));
        }

        //Geri Yükle
        private GridViewColumnHeader listViewRestoreCol = null;
        private SortAdorner listViewRestoreAdorner = null;

        private void listviewRestoreColumnHeaderSort_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewRestoreCol != null)
            {
                AdornerLayer.GetAdornerLayer(listViewRestoreCol).Remove(listViewRestoreAdorner);
                listViewRestore.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewRestoreCol == column && listViewRestoreAdorner.Direction == newDir)
                newDir = ListSortDirection.Descending;

            listViewRestoreCol = column;
            listViewRestoreAdorner = new SortAdorner(listViewRestoreCol, newDir);
            AdornerLayer.GetAdornerLayer(listViewRestoreCol).Add(listViewRestoreAdorner);
            listViewRestore.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));
        }

        //Görevler
        private GridViewColumnHeader listViewTasksCol = null;
        private SortAdorner listViewTasksAdorner = null;

        private void listviewTasksColumnHeaderSort_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewTasksCol != null)
            {
                AdornerLayer.GetAdornerLayer(listViewTasksCol).Remove(listViewTasksAdorner);
                listViewTasks.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewTasksCol == column && listViewTasksAdorner.Direction == newDir)
                newDir = ListSortDirection.Descending;

            listViewTasksCol = column;
            listViewTasksAdorner = new SortAdorner(listViewTasksCol, newDir);
            AdornerLayer.GetAdornerLayer(listViewTasksCol).Add(listViewTasksAdorner);
            listViewTasks.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));
        }





        #endregion

    }
}
