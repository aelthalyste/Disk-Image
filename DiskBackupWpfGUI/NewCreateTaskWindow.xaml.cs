using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler;
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
using System.Windows.Forms.VisualStyles;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for NewCreateTask.xaml
    /// </summary>
    public partial class NewCreateTaskWindow : Window
    {
        private IBackupService _backupService;
        private IBackupStorageService _backupStorageService;

        private readonly IBackupTaskDal _backupTaskDal;
        private readonly IStatusInfoDal _statusInfoDal;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly IBackupStorageDal _backupStorageDal;
        private ITaskSchedulerManager _schedulerManager;

        private readonly ILifetimeScope _scope;
        private readonly ILogger _logger;

        private List<BackupStorageInfo> _backupStorageInfoList = new List<BackupStorageInfo>();
        private List<VolumeInfo> _volumeInfoList = new List<VolumeInfo>();

        private TaskInfo _taskInfo = new TaskInfo();

        private readonly Func<AddBackupAreaWindow> _createAddBackupWindow;
        private bool _updateControl = false;

        public NewCreateTaskWindow(List<BackupStorageInfo> backupStorageInfoList, IBackupService backupService, IBackupStorageService backupStorageService,
            Func<AddBackupAreaWindow> createAddBackupWindow, List<VolumeInfo> volumeInfoList, IBackupTaskDal backupTaskDal, IStatusInfoDal statusInfoDal,
            ITaskInfoDal taskInfoDal, ITaskSchedulerManager schedulerManager, ILifetimeScope scope, ILogger logger)
        {
            InitializeComponent();

            _backupStorageInfoList = backupStorageInfoList;
            cbTargetBackupArea.ItemsSource = _backupStorageInfoList;
            _backupService = backupService;
            _backupStorageService = backupStorageService;
            _createAddBackupWindow = createAddBackupWindow;
            _volumeInfoList = volumeInfoList;
            _backupTaskDal = backupTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;
            _taskInfo.BackupTaskInfo = new BackupTask();
            _taskInfo.StatusInfo = new StatusInfo();
            _schedulerManager = schedulerManager;
            _scope = scope;
            _logger = logger.ForContext<NewCreateTaskWindow>();
            _schedulerManager.InitShedulerAsync();

            txtTaskName.Focus();

            _taskInfo.Obje = _volumeInfoList.Count();

            foreach (var item in _volumeInfoList)
            {
                _taskInfo.StrObje += item.Letter;
                lblBackupStorages.Text += (item.Letter + ", ");
            }

            lblBackupStorages.Text = lblBackupStorages.Text.Substring(0, lblBackupStorages.Text.Length - 2);
        }

        public NewCreateTaskWindow(List<BackupStorageInfo> backupStorageInfoList, IBackupService backupService, IBackupStorageService backupStorageService,
            Func<AddBackupAreaWindow> createAddBackupWindow, List<VolumeInfo> volumeInfoList, IBackupTaskDal backupTaskDal, IStatusInfoDal statusInfoDal,
            ITaskInfoDal taskInfoDal, ITaskSchedulerManager schedulerManager, ILifetimeScope scope, TaskInfo taskInfo, IBackupStorageDal backupStorageDal, ILogger logger)
        {
            InitializeComponent();

            _backupStorageInfoList = backupStorageInfoList;
            cbTargetBackupArea.ItemsSource = _backupStorageInfoList;
            _backupService = backupService;
            _backupStorageService = backupStorageService;
            _createAddBackupWindow = createAddBackupWindow;
            _volumeInfoList = volumeInfoList;
            _backupTaskDal = backupTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;

            _updateControl = true;
            _backupStorageDal = backupStorageDal;
            _taskInfo = taskInfo;
            _taskInfo.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == _taskInfo.BackupTaskId);
            _taskInfo.BackupStorageInfo = _backupStorageDal.Get(x => x.Id == _taskInfo.BackupStorageInfoId);
            _taskInfo.StatusInfo = _statusInfoDal.Get(x => x.Id == _taskInfo.StatusInfoId);
            _schedulerManager = schedulerManager;
            _scope = scope;
            _logger = logger.ForContext<NewCreateTaskWindow>();
            _schedulerManager.InitShedulerAsync();

            txtTaskName.Text = _taskInfo.Name;
            txtTaskDescription.Text = _taskInfo.Descripiton;

            if (_taskInfo.BackupTaskInfo.Type == BackupTypes.Diff)
                rbBTDifferential.IsChecked = true;
            else if (_taskInfo.BackupTaskInfo.Type == BackupTypes.Inc)
                rbBTIncremental.IsChecked = true;
            else
                rbBTFull.IsChecked = true;

            foreach (BackupStorageInfo item in cbTargetBackupArea.Items)
            {
                if (item.Id == _taskInfo.BackupStorageInfoId)
                    cbTargetBackupArea.SelectedItem = item;
            }

            if (_taskInfo.BackupTaskInfo.AutoRun) // otomatik çalıştır aktif
            {
                checkAutoRun.IsChecked = true;
                if (_taskInfo.BackupTaskInfo.AutoType == AutoRunType.DaysTime)
                {
                    rbDaysTime.IsChecked = true;
                    tpDaysTime.Value = _taskInfo.NextDate;
                    if (_taskInfo.ScheduleId.Contains("Everyday"))
                        cbDaysTime.SelectedIndex = 0;
                    else if (_taskInfo.ScheduleId.Contains("WeekDays"))
                        cbDaysTime.SelectedIndex = 1;
                    else
                    {
                        cbDaysTime.SelectedIndex = 2;
                    }
                }
                else if (_taskInfo.BackupTaskInfo.AutoType == AutoRunType.WeeklyTime)
                {
                    rbWeeklyTime.IsChecked = true;
                    tpWeeklyTime.Value = _taskInfo.NextDate;
                    cbWeeklyTimeWeek.SelectedIndex = Convert.ToInt32(_taskInfo.BackupTaskInfo.WeeklyTime) - 1;
                    cbWeeklyTimeDays.SelectedIndex = Convert.ToInt32(_taskInfo.BackupTaskInfo.Days) - 1;
                }
                else if (_taskInfo.BackupTaskInfo.AutoType == AutoRunType.Periodic)
                {
                    rbPeriodic.IsChecked = true;
                    txtPeriodic.Text = _taskInfo.BackupTaskInfo.PeriodicTime.ToString();
                    if (_taskInfo.BackupTaskInfo.PeriodicTimeType == PeriodicType.Minute)
                        cbPeriodicTime.SelectedIndex = 1;
                    else
                        cbPeriodicTime.SelectedIndex = 0;
                }
                else
                {
                    _taskInfo.BackupTaskInfo.Months = null;
                }
            }
            else // otomatik çalıştır aktif değil
            {
                _taskInfo.NextDate = Convert.ToDateTime("01/01/0002");
            }

            if (_taskInfo.BackupTaskInfo.FailTryAgain) // başarısız tekrar dene aktif
            {
                checkTimeFailDesc.IsChecked = true;
                txtTimeFailDesc.Text = _taskInfo.BackupTaskInfo.FailNumberTryAgain.ToString();
                txtTimeWait.Text = _taskInfo.BackupTaskInfo.WaitNumberTryAgain.ToString();
            }

            txtTaskName.Focus();

            foreach (var item in _taskInfo.StrObje)
            {
                lblBackupStorages.Text += (item + ", ");
            }

            lblBackupStorages.Text = lblBackupStorages.Text.Substring(0, lblBackupStorages.Text.Length - 2);

            _taskInfo.BackupTaskInfo.FailNumberTryAgain = 0;
            _taskInfo.BackupTaskInfo.WaitNumberTryAgain = 0;
            _taskInfo.BackupTaskInfo.AutoType = 0;
            //_taskInfo.BackupTaskInfo.Days = null;
            _taskInfo.BackupTaskInfo.WeeklyTime = WeeklyType.Unset;
            _taskInfo.BackupTaskInfo.PeriodicTime = 0;
            //_taskInfo.BackupTaskInfo.Months = null;
        }

        #region Title Bar
        private void btnNewCreateTaskClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void btnNewCreateTaskMin_Click(object sender, RoutedEventArgs e)
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
        private void btnCreateTaskOk_Click(object sender, RoutedEventArgs e)
        {
            if (ConfirmNotEmpty())
            {
                MessageBox.Show("İlgili alanları lütfen boş geçmeyiniz.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else
            {
                // kaydet
                _taskInfo.Type = TaskType.Backup;
                _taskInfo.Name = txtTaskName.Text;
                _taskInfo.Descripiton = txtTaskDescription.Text;
                _taskInfo.BackupTaskInfo.TaskName = txtTaskName.Text;

                if (rbBTDifferential.IsChecked.Value) // diff
                {
                    _taskInfo.BackupTaskInfo.Type = BackupTypes.Diff;
                }
                else if (rbBTIncremental.IsChecked.Value) // inc
                {
                    _taskInfo.BackupTaskInfo.Type = BackupTypes.Inc;
                }
                else if (rbBTFull.IsChecked.Value) // full
                {
                    _taskInfo.BackupTaskInfo.Type = BackupTypes.Full;
                }

                // hedefdeki retentiontime vs
                _taskInfo.BackupTaskInfo.RetentionTime = Convert.ToInt32(txtRetentionTime.Text);
                _taskInfo.BackupTaskInfo.FullOverwrite = chbFullOverwrite.IsChecked.Value;
                _taskInfo.BackupTaskInfo.FullBackup = Convert.ToInt32(txtFullBackup.Text);
                if (_taskInfo.BackupStorageInfo.IsCloud)
                {
                    _taskInfo.BackupTaskInfo.NarRetentionTime = Convert.ToInt32(txtNarRetentionTime.Text);
                    _taskInfo.BackupTaskInfo.NarFullOverwrite = chbNarFullOverwrite.IsChecked.Value;
                    _taskInfo.BackupTaskInfo.NarFullBackup = Convert.ToInt32(txtNarFullBackup.Text);
                }

                // zamanlama
                _taskInfo.BackupTaskInfo.AutoRun = checkAutoRun.IsChecked.Value;
                if (checkAutoRun.IsChecked.Value)
                {
                    //radio buton değerler işlenecek
                    if (rbDaysTime.IsChecked.Value)
                    {
                        _taskInfo.BackupTaskInfo.AutoType = AutoRunType.DaysTime;
                        _taskInfo.NextDate = (DateTime)tpDaysTime.Value;
                        Console.WriteLine("Window: " + _taskInfo.NextDate.Hour + " saat" + _taskInfo.NextDate.Minute + " dakika");

                        if (cbDaysTime.SelectedIndex == 2) // belirli günler seçilmeli
                        {
                            _taskInfo.BackupTaskInfo.Days = ChooseDayAndMounthsWindow._days;
                        }
                        else
                        {
                            _taskInfo.BackupTaskInfo.Days = null;
                        }
                    }
                    else if (rbWeeklyTime.IsChecked.Value)
                    {
                        _taskInfo.BackupTaskInfo.AutoType = AutoRunType.WeeklyTime;
                        _taskInfo.BackupTaskInfo.Months = ChooseDayAndMounthsWindow._months;
                        _taskInfo.NextDate = (DateTime)tpWeeklyTime.Value;
                        //haftalar
                        if (cbWeeklyTimeWeek.SelectedIndex == 0)
                        {
                            _taskInfo.BackupTaskInfo.WeeklyTime = WeeklyType.First;
                        }
                        else if (cbWeeklyTimeWeek.SelectedIndex == 1)
                        {
                            _taskInfo.BackupTaskInfo.WeeklyTime = WeeklyType.Second;
                        }
                        else if (cbWeeklyTimeWeek.SelectedIndex == 2)
                        {
                            _taskInfo.BackupTaskInfo.WeeklyTime = WeeklyType.Third;
                        }
                        else
                        {
                            _taskInfo.BackupTaskInfo.WeeklyTime = WeeklyType.Fourth;
                        }
                        //günler
                        _taskInfo.BackupTaskInfo.Days = (cbWeeklyTimeDays.SelectedIndex + 1).ToString();
                    }
                    else if (rbPeriodic.IsChecked.Value)
                    {
                        _taskInfo.BackupTaskInfo.AutoType = AutoRunType.Periodic;
                        _taskInfo.BackupTaskInfo.PeriodicTime = Convert.ToInt32(txtPeriodic.Text);
                        if (cbPeriodicTime.SelectedIndex == 0)
                        {
                            _taskInfo.BackupTaskInfo.PeriodicTimeType = PeriodicType.Hour;
                        }
                        else
                        {
                            _taskInfo.BackupTaskInfo.PeriodicTimeType = PeriodicType.Minute;
                        }
                    }
                    else
                    {
                        _taskInfo.BackupTaskInfo.Months = null;
                    }
                }
                else
                {
                    _taskInfo.NextDate = Convert.ToDateTime("01/01/0002");
                }

                //başarısız tekrar dene
                _taskInfo.BackupTaskInfo.FailTryAgain = checkTimeFailDesc.IsChecked.Value;
                if (checkTimeFailDesc.IsChecked.Value)
                {
                    _taskInfo.BackupTaskInfo.FailNumberTryAgain = Convert.ToInt32(txtTimeFailDesc.Text);
                    _taskInfo.BackupTaskInfo.WaitNumberTryAgain = Convert.ToInt32(txtTimeWait.Text);
                }

                // Zincir kontrolü
                if (!ChainCheck())
                {
                    Close();
                    return;
                }

                //veritabanı işlemleri
                TaskInfo resultTaskInfo = new TaskInfo();
                if (!_updateControl)
                {
                    resultTaskInfo = SaveToDatabase();
                }
                else
                {
                    // schedulerı sileceksin -- _taskInfo.schedulerId sil
                    if (_taskInfo.ScheduleId != null && !_taskInfo.ScheduleId.Contains("Now") && _taskInfo.ScheduleId != "")
                    {
                        var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                        taskSchedulerManager.DeleteJob(_taskInfo.ScheduleId);
                    }
                    _taskInfo.ScheduleId = "";
                    _taskInfo.EnableDisable = TecnicalTaskStatusType.Enable;

                    //Zincir temizlemeye gerek var mı kontrolü
                    var resultBackupTask = _backupTaskDal.Get(x => x.Id == _taskInfo.BackupTaskId);
                    if (resultBackupTask.Type != _taskInfo.BackupTaskInfo.Type)
                    {
                        _logger.Information("{harf} zincir temizleniyor. {oncekiTip} tipinden, {yeniTip} tipine düzenleme yapıldı.", _taskInfo.StrObje, Resources[resultBackupTask.Type.ToString()], Resources[_taskInfo.BackupTaskInfo.Type.ToString()]);
                        foreach (var item in _taskInfo.StrObje)
                        {
                            _backupService.CleanChain(item);
                        }
                    }

                    // güncelleme işlemi yapacaksın
                    resultTaskInfo = UpdateToDatabase();
                }


                if (resultTaskInfo != null)
                {
                    if (resultTaskInfo.BackupTaskInfo.Type == BackupTypes.Diff || resultTaskInfo.BackupTaskInfo.Type == BackupTypes.Inc)
                    {
                        if (resultTaskInfo.BackupTaskInfo.AutoRun)
                        {
                            if (resultTaskInfo.BackupTaskInfo.AutoType == AutoRunType.DaysTime)
                            {
                                if (cbDaysTime.SelectedIndex == 0) // everyday
                                {
                                    _schedulerManager.BackupIncDiffEverydayJob(resultTaskInfo).Wait();
                                }
                                else if (cbDaysTime.SelectedIndex == 1) //weekdays
                                {
                                    _schedulerManager.BackupIncDiffWeekDaysJob(resultTaskInfo).Wait();
                                }
                                else //certain
                                {
                                    _schedulerManager.BackupIncDiffCertainDaysJob(resultTaskInfo).Wait();
                                }
                            }
                            else if (resultTaskInfo.BackupTaskInfo.AutoType == AutoRunType.WeeklyTime)
                            {
                                _schedulerManager.BackupIncDiffWeeklyJob(resultTaskInfo).Wait();
                            }
                            else //periodic
                            {
                                if (cbPeriodicTime.SelectedIndex == 0) //saat
                                {
                                    _schedulerManager.BackupIncDiffPeriodicHoursJob(resultTaskInfo).Wait();
                                }
                                else //dakika
                                {
                                    _schedulerManager.BackupIncDiffPeriodicMinutesJob(resultTaskInfo).Wait();
                                }
                            }
                        }
                    }
                    else
                    {
                        //full gelince buraya alıcaz paşayı
                    }

                    Close();

                    if (_updateControl)
                        MessageBox.Show("Güncelleme başarılı.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Information);
                    else
                        MessageBox.Show("Ekleme başarılı.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                else
                {
                    if (_updateControl)
                        MessageBox.Show("Güncelleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                    else
                        MessageBox.Show("Ekleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                }


                // task status açılması
                //resultTaskInfo.StatusInfo = _statusInfoDal.Get(x => x.Id == resultTaskInfo.StatusInfoId);
                //resultTaskInfo.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == resultTaskInfo.BackupTaskId);
                //StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("statusInfo", resultTaskInfo.StatusInfo));
                //backupStatus.Show();
            }

        }

        private bool ChainCheck()
        {
            List<TaskInfo> brokenTaskList = new List<TaskInfo>();
            List<TaskInfo> taskList = new List<TaskInfo>();
            if (!_updateControl)
                taskList = _taskInfoDal.GetList(x => x.Type == TaskType.Backup && x.EnableDisable != TecnicalTaskStatusType.Broken);
            else
                taskList = _taskInfoDal.GetList(x => x.Type == TaskType.Backup && x.EnableDisable != TecnicalTaskStatusType.Broken && x.Id != _taskInfo.Id);

            foreach (var itemTask in taskList)
            {
                itemTask.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == itemTask.BackupTaskId);
                if (itemTask.BackupTaskInfo.Type != _taskInfo.BackupTaskInfo.Type) // ezicek bir tür var
                {
                    foreach (var item in itemTask.StrObje) // C D E
                    {
                        if (_taskInfo.StrObje.Contains(item))
                        {
                            brokenTaskList.Add(itemTask);
                            break;
                        }
                    }
                }
            }

            if (brokenTaskList.Count > 0) // bozulacak görevler var
            {
                foreach (var item in brokenTaskList)
                {
                    Console.WriteLine(item.Name);
                }
                var result = MessageBox.Show("Oluşturmak istediğiniz görevlerin, volumelarında işlemekte olan görevleriniz etkisiz hale getirilecek.\nOnaylıyor musunuz?", "Narbulut diyor ki; ", MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
                if (result == MessageBoxResult.No)
                {
                    return false;
                }
                else
                {
                    _logger.Information("Kullanıcının onayı doğrultusunda işlemekte olan diğer görevler etkisiz hale getiriliyor.");
                    foreach (var itemTask in brokenTaskList)
                    {
                        foreach (var item in itemTask.StrObje)
                        {
                            _backupService.CleanChain(item);
                        }
                        itemTask.EnableDisable = TecnicalTaskStatusType.Broken;

                        if (itemTask.ScheduleId != null && itemTask.ScheduleId != "")
                        {
                            var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                            taskSchedulerManager.DeleteJob(itemTask.ScheduleId);
                            itemTask.ScheduleId = "";
                        }

                        _taskInfoDal.Update(itemTask);
                    }
                }
            }
            return true;
        }

        private void btnCreateTaskBack_Click(object sender, RoutedEventArgs e)
        {
            if (NCTTabControl.SelectedIndex != 0)
            {
                if (NCTTabControl.SelectedIndex == 5)
                {
                    NCTTabControl.SelectedIndex -= 1;
                }
                NCTTabControl.SelectedIndex -= 1;
            }
        }

        private void btnCreateTaskNext_Click(object sender, RoutedEventArgs e)
        {
            if (NCTTabControl.SelectedIndex != 5)
            {
                if (NCTTabControl.SelectedIndex == 3)
                {
                    NCTTabControl.SelectedIndex += 1;
                }
                NCTTabControl.SelectedIndex += 1;
            }

            // özet yazımı
            if (NCTTabControl.SelectedIndex == 5)
            {
                lblTaskName.Text = txtTaskName.Text;

                if (rbBTDifferential.IsChecked.Value) // diff
                {
                    lblBackupType.Text = Resources["diff"].ToString();
                }
                else if (rbBTIncremental.IsChecked.Value) // inc
                {
                    lblBackupType.Text = Resources["inc"].ToString();
                }
                else if (rbBTFull.IsChecked.Value) // full
                {
                    lblBackupType.Text = Resources["full"].ToString();
                }

                if (checkAutoRun.IsChecked.Value) // otomatik çalıştır aktif ise
                {
                    if (rbDaysTime.IsChecked.Value) // günlük
                    {
                        lblWorkingTimeTask.Text = Resources["dailyTime"].ToString();
                    }
                    else if (rbWeeklyTime.IsChecked.Value) // haftalık
                    {
                        lblWorkingTimeTask.Text = Resources["weeklyTime"].ToString();
                    }
                    else if (rbPeriodic.IsChecked.Value) // periyodik
                    {
                        lblWorkingTimeTask.Text = Resources["periodic"].ToString();
                    }
                }
                else
                {
                    lblWorkingTimeTask.Text = Resources["NCTuntimelyTask"].ToString();
                }
            }
        }
        private void btnCreateTaskCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        #endregion

        #region Name Tab

        #endregion

        #region Backup Type Tab

        #endregion

        #region Target Type Tab

        private void cbTargetBackupArea_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (cbTargetBackupArea.SelectedIndex != -1)
            {
                foreach (var item in _backupStorageInfoList)
                {
                    if (((BackupStorageInfo)cbTargetBackupArea.SelectedItem).Id == item.Id)
                    {
                        //yerel disk - nas
                        lblTargetTotalSize.Text = item.StrCapacity;
                        lblTargetFreeSize.Text = item.StrFreeSize;
                        lblTargetFullSize.Text = item.StrUsedSize;
                        // pasta işlemleri
                        double Capacity = item.Capacity;
                        double UsedSize = item.UsedSize;
                        if (UsedSize != 0)
                        {
                            var diskRatio = Capacity / UsedSize;
                            var pieRatio = 360 / diskRatio;

                            pieDiskSize.EndAngle = -90 + pieRatio;
                        }
                        else
                        {
                            pieDiskSize.EndAngle = -89;
                        }

                        //cloud
                        if (item.IsCloud)
                        {
                            gridIsCloud.Visibility = Visibility.Visible;
                            lblTargetNarbulutTotalSize.Text = item.StrCloudCapacity;
                            lblTargetNarbulutFreeSize.Text = item.StrCloudFreeSize;
                            lblTargetNarbulutFullSize.Text = item.StrCloudUsedSize;
                            // pasta işlemleri
                            double cloudCapacity = item.CloudCapacity;
                            double cloudUsedSize = item.CloudUsedSize;
                            if (cloudUsedSize != 0)
                            {
                                var diskRatio = cloudCapacity / cloudUsedSize;
                                var pieRatio = 360 / diskRatio;

                                pieCloudSize.EndAngle = -90 + pieRatio;
                            }
                            else
                            {
                                pieCloudSize.EndAngle = -89;
                            }
                        }
                        else
                        {
                            gridIsCloud.Visibility = Visibility.Hidden;
                        }

                        _taskInfo.BackupStorageInfo = item;
                        _taskInfo.BackupStorageInfoId = item.Id;

                        break;
                    }
                }
            }
        }

        private void btnTargetAdd_Click(object sender, RoutedEventArgs e)
        {
            AddBackupAreaWindow addBackupArea = _createAddBackupWindow();
            addBackupArea.ShowDialog();

            //karşılaştırma yapıp ekleneni yeniden gösteriyoruz
            List<DiskInformation> diskList = _backupService.GetDiskList();
            List<VolumeInfo> volumeList = new List<VolumeInfo>();

            foreach (var diskItem in diskList)
            {
                foreach (var volumeItem in diskItem.VolumeInfos)
                {
                    volumeList.Add(volumeItem);
                }
            }

            _backupStorageInfoList = MainWindow.GetBackupStorages(volumeList, _backupStorageService.BackupStorageInfoList());
            cbTargetBackupArea.ItemsSource = _backupStorageInfoList;
        }

        #region Arrow Button
        private void btnRetentionUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtRetentionTime.Text);
            if (count != 999)
            {
                count += 1;
                txtRetentionTime.Text = count.ToString();
            }
        }

        private void btnFullBackupUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtFullBackup.Text);
            if (count != 999)
            {
                count += 1;
                txtFullBackup.Text = count.ToString();
            }
        }

        private void btnNarRetentionUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtNarRetentionTime.Text);
            if (count != 999)
            {
                count += 1;
                txtNarRetentionTime.Text = count.ToString();
            }
        }

        private void btnNarFullBackupUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtNarFullBackup.Text);
            if (count != 999)
            {
                count += 1;
                txtNarFullBackup.Text = count.ToString();
            }
        }

        private void btnRetentionDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtRetentionTime.Text);
            if (count != 0)
            {
                count -= 1;
                txtRetentionTime.Text = count.ToString();
            }
        }

        private void btnFullBackupDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtFullBackup.Text);
            if (count != 0)
            {
                count -= 1;
                txtFullBackup.Text = count.ToString();
            }
        }

        private void btnNarRetentionDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtNarRetentionTime.Text);
            if (count != 0)
            {
                count -= 1;
                txtNarRetentionTime.Text = count.ToString();
            }
        }

        private void btnNarFullBackupDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtNarFullBackup.Text);
            if (count != 0)
            {
                count -= 1;
                txtNarFullBackup.Text = count.ToString();
            }
        }
        #endregion


        #endregion

        #region Schuduler Tab

        #region Radio Button
        private void rbDaysTime_Unchecked(object sender, RoutedEventArgs e)
        {
            stackDaysTime.IsEnabled = false;
        }

        private void rbDaysTime_Checked(object sender, RoutedEventArgs e)
        {
            stackDaysTime.IsEnabled = true;
        }

        private void rbWeeklyTime_Checked(object sender, RoutedEventArgs e)
        {
            stackWeeklyTime.IsEnabled = true;
        }

        private void rbWeeklyTime_Unchecked(object sender, RoutedEventArgs e)
        {
            stackWeeklyTime.IsEnabled = false;
        }

        private void rbPeriodic_Checked(object sender, RoutedEventArgs e)
        {
            stackPeriodic.IsEnabled = true;
        }

        private void rbPeriodic_Unchecked(object sender, RoutedEventArgs e)
        {
            stackPeriodic.IsEnabled = false;
        }
        #endregion

        #region CheckBox
        private void checkAutoRun_Checked(object sender, RoutedEventArgs e)
        {
            stackAutoRun.IsEnabled = true;
        }

        private void checkAutoRun_Unchecked(object sender, RoutedEventArgs e)
        {
            stackAutoRun.IsEnabled = false;
            // sıfırlama
            rbDaysTime.IsChecked = true;
            rbDaysTime.IsChecked = false;
        }

        private void checkTimeFailDesc_Unchecked(object sender, RoutedEventArgs e)
        {
            stackTimeFailDesc.IsEnabled = false;
            stackTimeWait.IsEnabled = false;
        }

        private void checkTimeFailDesc_Checked(object sender, RoutedEventArgs e)
        {
            stackTimeFailDesc.IsEnabled = true;
            stackTimeWait.IsEnabled = true;
        }
        #endregion

        #region Arrow Button
        private void btnTimeFailDescUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtTimeFailDesc.Text);
            if (count != 999)
            {
                count += 1;
                txtTimeFailDesc.Text = count.ToString();
            }
        }

        private void btnTimeFailDescDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtTimeFailDesc.Text);
            if (count != 0)
            {
                count -= 1;
                txtTimeFailDesc.Text = count.ToString();
            }
        }

        private void btnTimeWaitUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtTimeWait.Text);
            if (count != 999)
            {
                count += 1;
                txtTimeWait.Text = count.ToString();
            }
        }

        private void btnTimeWaitDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtTimeWait.Text);
            if (count != 0)
            {
                count -= 1;
                txtTimeWait.Text = count.ToString();
            }
        }

        private void btnPeriodicDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtPeriodic.Text);
            if (count != 0)
            {
                count -= 1;
                txtPeriodic.Text = count.ToString();
            }
        }

        private void btnPeriodicUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtPeriodic.Text);
            if (count != 999)
            {
                count += 1;
                txtPeriodic.Text = count.ToString();
            }
        }
        #endregion

        private void btnDaysTimeDays_Click(object sender, RoutedEventArgs e)
        {
            //ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(true);
            //chooseDays.ShowDialog();
            if (_taskInfo.BackupTaskInfo.Days == null)
            {
                ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(true);
                chooseDays.ShowDialog();
                _taskInfo.BackupTaskInfo.Days = ChooseDayAndMounthsWindow._days;
            }
            else
            {
                // doldurma yap
                ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(true, _taskInfo.BackupTaskInfo.Days, _updateControl);
                chooseDays.ShowDialog();
                _taskInfo.BackupTaskInfo.Days = ChooseDayAndMounthsWindow._days;
            }
        }

        private void btnWeeklyTimeWeek_Click(object sender, RoutedEventArgs e)
        {
            //ChooseDayAndMounthsWindow chooseMounths = new ChooseDayAndMounthsWindow(false);
            //chooseMounths.ShowDialog();
            if (_taskInfo.BackupTaskInfo.Months == null)
            {
                ChooseDayAndMounthsWindow chooseMounths = new ChooseDayAndMounthsWindow(false);
                chooseMounths.ShowDialog();
                _taskInfo.BackupTaskInfo.Months = ChooseDayAndMounthsWindow._months;
            }
            else
            {
                ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(false, _taskInfo.BackupTaskInfo.Months, _updateControl);
                chooseDays.ShowDialog();
                _taskInfo.BackupTaskInfo.Months = ChooseDayAndMounthsWindow._months;
            }
        }

        private bool _daysBtnControl = false;
        private void cbDaysTime_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (cbDaysTime.SelectedIndex == 2)
            {
                btnDaysTimeDays.IsEnabled = true;
                _daysBtnControl = true;
            }
            else
            {
                if (_daysBtnControl) //btnDaysTimeDays.IsEnabled sorulacak booldan kaçamadık null aldık
                {
                    btnDaysTimeDays.IsEnabled = false;
                    _daysBtnControl = false;
                }
            }
        }



        #endregion

        #region Settings Tab

        #endregion

        #region Summary Tab

        #endregion

        private void NCTTabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (NCTTabControl.SelectedIndex == 0)
            {
                lblTabHeader.Text = Resources["name"].ToString();
                lblTabContent.Text = Resources["NCTNameContent"].ToString();
                btnCreateTaskBack.IsEnabled = false;
            }
            else if (NCTTabControl.SelectedIndex == 1)
            {
                lblTabHeader.Text = Resources["BackupType"].ToString();
                lblTabContent.Text = Resources["NCTBackupTypeContent"].ToString();
                btnCreateTaskBack.IsEnabled = true;
            }
            else if (NCTTabControl.SelectedIndex == 2)
            {
                lblTabHeader.Text = Resources["target"].ToString();
                lblTabContent.Text = Resources["NCTTargetContent"].ToString();
            }
            else if (NCTTabControl.SelectedIndex == 3)
            {
                lblTabHeader.Text = Resources["scheduler"].ToString();
                lblTabContent.Text = Resources["NCTSchedulerContent"].ToString();
            }
            else if (NCTTabControl.SelectedIndex == 4)
            {
                lblTabHeader.Text = Resources["settings"].ToString();
                lblTabContent.Text = Resources["NCTSettingsContent"].ToString();
                btnCreateTaskNext.IsEnabled = true;
            }
            else if (NCTTabControl.SelectedIndex == 5)
            {
                lblTabHeader.Text = Resources["summary"].ToString();
                lblTabContent.Text = Resources["NCTSummaryContent"].ToString();
                btnCreateTaskNext.IsEnabled = false;
            }
        }

        private bool ConfirmNotEmpty()
        {
            bool errorFlag = false;

            // boş geçilmeme kontrolü
            if (txtTaskName.Text.Equals("") || cbTargetBackupArea.SelectedIndex == -1 || txtRetentionTime.Text.Equals("") ||
                txtFullBackup.Text.Equals("") || txtNarRetentionTime.Text.Equals("") || txtNarFullBackup.Text.Equals(""))
            {
                errorFlag = true;
            }
            else if (checkAutoRun.IsChecked.Value) // zamanlama tabı için
            {
                if (rbDaysTime.IsChecked.Value)
                {
                    if (tpDaysTime.Value.ToString().Equals(""))
                    {
                        errorFlag = true;
                    }
                    else if (cbDaysTime.SelectedIndex == 2) // belirli günler seçilmeli
                    {
                        if (ChooseDayAndMounthsWindow._days == null)
                        {
                            errorFlag = true;
                        }
                    }
                }
                else if (rbWeeklyTime.IsChecked.Value)
                {
                    if (tpWeeklyTime.Value.ToString().Equals(""))
                    {
                        errorFlag = true;
                    }
                    else // aylar seçilmeli
                    {
                        if (ChooseDayAndMounthsWindow._months == null)
                        {
                            errorFlag = true;
                        }
                    }
                }
                else if (rbPeriodic.IsChecked.Value)
                {
                    if (txtPeriodic.Text.Equals(""))
                    {
                        errorFlag = true;
                    }
                }
                else // radiobuttonlar seçili değil
                {
                    errorFlag = true;
                }
            }
            if (checkTimeFailDesc.IsChecked.Value)
            {
                if (txtTimeFailDesc.Text.Equals("") || txtTimeWait.Text.Equals(""))
                {
                    errorFlag = true;
                }
            }

            return errorFlag;
        }

        private TaskInfo SaveToDatabase()
        {
            //backupTask kaydetme
            var resultBackupTask = _backupTaskDal.Add(_taskInfo.BackupTaskInfo);
            if (resultBackupTask == null)
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
            _taskInfo.Status = TaskStatusType.FirstMissionExpected;
            _taskInfo.StatusInfoId = resultStatusInfo.Id;
            _taskInfo.BackupTaskId = resultBackupTask.Id;
            _taskInfo.LastWorkingDate = Convert.ToDateTime("01/01/0002");
            var resultTaskInfo = _taskInfoDal.Add(_taskInfo);
            if (resultTaskInfo == null)
            {
                MessageBox.Show("Ekleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            return resultTaskInfo;
        }

        private TaskInfo UpdateToDatabase()
        {
            //backupTask kaydetme
            var resultBackupTask = _backupTaskDal.Update(_taskInfo.BackupTaskInfo);
            if (resultBackupTask == null)
            {
                MessageBox.Show("Güncelleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            //backupTask kaydetme
            _taskInfo.StatusInfo.TaskName = _taskInfo.Name;
            _taskInfo.StatusInfo.SourceObje = _taskInfo.StrObje;
            var resultStatusInfo = _statusInfoDal.Update(_taskInfo.StatusInfo);
            if (resultStatusInfo == null)
            {
                MessageBox.Show("Güncelleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            // task kayıdı
            _taskInfo.Status = TaskStatusType.FirstMissionExpected;
            _taskInfo.StatusInfoId = resultStatusInfo.Id;
            _taskInfo.BackupTaskId = resultBackupTask.Id;
            _taskInfo.LastWorkingDate = Convert.ToDateTime("01/01/0002");
            var resultTaskInfo = _taskInfoDal.Update(_taskInfo);
            if (resultTaskInfo == null)
            {
                MessageBox.Show("Güncelleme başarısız.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            return resultTaskInfo;
        }

        #region Focus Event
        private void txtTaskName_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                txtTaskDescription.Focus();
            }
        }

        private void cbTargetBackupArea_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                btnCreateTaskNext.Focus();
            }
        }
        #endregion
    }
}
