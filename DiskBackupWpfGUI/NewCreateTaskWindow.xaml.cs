using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
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
        private readonly IRestoreTaskDal _restoreTaskDal;
        private ITaskSchedulerManager _schedulerManager;
        private readonly ILifetimeScope _scope;
        private readonly ILogger _logger;
        private readonly IConfigurationDataDal _configurationDataDal;

        private List<BackupStorageInfo> _backupStorageInfoList = new List<BackupStorageInfo>();

        private TaskInfo _taskInfo = new TaskInfo();

        private readonly Func<AddBackupAreaWindow> _createAddBackupWindow;
        private bool _updateControl = false;
        public bool _showTaskTab = false;

        public NewCreateTaskWindow(List<BackupStorageInfo> backupStorageInfoList, IBackupService backupService, IBackupStorageService backupStorageService,
            Func<AddBackupAreaWindow> createAddBackupWindow, List<VolumeInfo> volumeInfoList, IBackupTaskDal backupTaskDal, IStatusInfoDal statusInfoDal,
            ITaskInfoDal taskInfoDal, ITaskSchedulerManager schedulerManager, ILifetimeScope scope, ILogger logger, IRestoreTaskDal restoreTaskDal, IConfigurationDataDal configurationDataDal)
        {
            InitializeComponent();

            _backupService = backupService;
            _backupStorageService = backupStorageService;
            _createAddBackupWindow = createAddBackupWindow;
            _backupTaskDal = backupTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;
            _restoreTaskDal = restoreTaskDal;
            _taskInfo.BackupTaskInfo = new BackupTask();
            _taskInfo.StatusInfo = new StatusInfo();
            _schedulerManager = schedulerManager;
            _scope = scope;
            _logger = logger.ForContext<NewCreateTaskWindow>();
            _schedulerManager.InitShedulerAsync();

            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            txtTaskName.Focus();

            _taskInfo.Obje = volumeInfoList.Count();

            foreach (var item in volumeInfoList)
            {
                _taskInfo.StrObje += item.Letter;
                lblBackupStorages.Text += (item.Letter + ", ");
            }

            lblBackupStorages.Text = lblBackupStorages.Text.Substring(0, lblBackupStorages.Text.Length - 2);

            GetBackupStorageList(backupStorageInfoList);
            if (_backupStorageInfoList.Count > 0)
                cbTargetBackupArea.SelectedIndex = 0;
        }

        public NewCreateTaskWindow(List<BackupStorageInfo> backupStorageInfoList, IBackupService backupService, IBackupStorageService backupStorageService,
            Func<AddBackupAreaWindow> createAddBackupWindow, IBackupTaskDal backupTaskDal, IStatusInfoDal statusInfoDal,
            ITaskInfoDal taskInfoDal, ITaskSchedulerManager schedulerManager, ILifetimeScope scope, TaskInfo taskInfo, IBackupStorageDal backupStorageDal, ILogger logger,
            IRestoreTaskDal restoreTaskDal, IConfigurationDataDal configurationDataDal)
        {
            InitializeComponent();

            _backupService = backupService;
            _backupStorageService = backupStorageService;
            _createAddBackupWindow = createAddBackupWindow;
            _backupTaskDal = backupTaskDal;
            _statusInfoDal = statusInfoDal;
            _taskInfoDal = taskInfoDal;
            _restoreTaskDal = restoreTaskDal;
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

            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            txtTaskName.Text = _taskInfo.Name;
            txtTaskDescription.Text = _taskInfo.Descripiton;

            if (_taskInfo.BackupTaskInfo.Type == BackupTypes.Diff)
                rbBTDifferential.IsChecked = true;
            else if (_taskInfo.BackupTaskInfo.Type == BackupTypes.Inc)
                rbBTIncremental.IsChecked = true;
            else
                rbBTFull.IsChecked = true;

            GetBackupStorageList(backupStorageInfoList);
            foreach (BackupStorageInfo item in cbTargetBackupArea.Items)
            {
                if (item.Id == _taskInfo.BackupStorageInfoId)
                    cbTargetBackupArea.SelectedItem = item;
            }

            if (_taskInfo.BackupTaskInfo.FullBackupTimeType == FullBackupTimeTyp.Day)
            {
                cbFullBackupTimeType.SelectedIndex = 0;
                for (int i = 0; i < cbFullBackupTime.Items.Count; i++)
                {
                    if (Convert.ToInt32(cbFullBackupTime.Items[i]) == _taskInfo.BackupTaskInfo.FullBackupTime)
                        cbFullBackupTime.SelectedIndex = i;
                }
            }
            else
            {
                cbFullBackupTimeType.SelectedIndex = 1;
                for (int i = 0; i < cbFullBackupTime.Items.Count; i++)
                {
                    if (Convert.ToInt32(cbFullBackupTime.Items[i]) == _taskInfo.BackupTaskInfo.FullBackupTime)
                        cbFullBackupTime.SelectedIndex = i;
                }
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

                    if (_taskInfo.BackupTaskInfo.PeriodicTimeType == PeriodicType.Minute)
                    {
                        cbPeriodicTime.SelectedIndex = 1;
                        for (int i = 0; i < cbPeriodic.Items.Count; i++)
                        {
                            if (Convert.ToInt32(cbPeriodic.Items[i]) == _taskInfo.BackupTaskInfo.PeriodicTime)
                                cbPeriodic.SelectedIndex = i;
                        }
                    }
                    else
                    {
                        cbPeriodicTime.SelectedIndex = 0;
                        for (int i = 0; i < cbPeriodic.Items.Count; i++)
                        {
                            if (Convert.ToInt32(cbPeriodic.Items[i]) == _taskInfo.BackupTaskInfo.PeriodicTime)
                                cbPeriodic.SelectedIndex = i;
                        }
                    }
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
            ChooseDayAndMounthsWindow._days = _taskInfo.BackupTaskInfo.Days;
            ChooseDayAndMounthsWindow._months = _taskInfo.BackupTaskInfo.Months;
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
                MessageBox.Show(Resources["notNullMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
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
                _taskInfo.BackupTaskInfo.FullBackupTime = Convert.ToInt32(cbFullBackupTime.SelectedItem);
                _taskInfo.BackupTaskInfo.FullBackupTimeType = (FullBackupTimeTyp)cbFullBackupTimeType.SelectedIndex;
                _taskInfo.BackupTaskInfo.FullOverwrite = chbFullOverwrite.IsChecked.Value;

                if (_taskInfo.BackupStorageInfo.IsCloud)
                {
                    // Eğer ki bulut kısmı açıldığında yetersiz alanda üzerine yaz vs. gibi özellikler geldiğinde buradan kayıt işlemi gerçekleştirilecek
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
                        _taskInfo.BackupTaskInfo.PeriodicTime = Convert.ToInt32(cbPeriodic.SelectedItem);
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
                /* artık aynı volumede bir backup görevi olacağı için inc-diff kontrolleri yapılmasına gerek kalmadı
                 * if (!ChainCheck())
                {
                    //Close(); // Kullanıcı reddettiğinde görev oluşturma ekranının tamamen kapanmasını engellemek için kaldır
                    return;
                }*/

                // Restore kontrolü
                var result = CheckAndBreakAffectedTask(_taskInfo);
                if (!result)
                {
                    //Close(); // Kullanıcı reddettiğinde görev oluşturma ekranının tamamen kapanmasını engellemek için kaldır
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
                            try
                            {
                                _backupService.CleanChain(item);
                            }
                            catch (Exception ex)
                            {
                                _logger.Error(ex, "Beklenmedik hatadan dolayı {harf} zincir temizleme işlemi gerçekleştirilemedi.", item);
                                MessageBox.Show(Resources["unexpectedError1MB"].ToString() + $" {item} " + Resources["notChainCleanMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                                Close();
                                return;
                            }
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
                        MessageBox.Show(Resources["updateSuccessMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                    else
                    {
                        _showTaskTab = true;
                        //MessageBox.Show(Resources["addSuccessMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                    }

                }
                else
                {
                    if (_updateControl)
                        MessageBox.Show(Resources["updateFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    else
                        MessageBox.Show(Resources["addFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }

        }

        private bool CheckAndBreakAffectedTask(TaskInfo taskInfo)
        {
            var taskList = _taskInfoDal.GetList(x => x.Type == TaskType.Restore && x.EnableDisable != TecnicalTaskStatusType.Broken);
            List<TaskInfo> taskAffectedList = new List<TaskInfo>();

            foreach (var item in taskList)
            {
                item.RestoreTaskInfo = _restoreTaskDal.Get(x => x.Id == item.RestoreTaskId);
                if (item.RestoreTaskInfo.Type == RestoreType.RestoreDisk)
                    taskAffectedList.Add(item);
            }

            bool checkFlag = false;
            foreach (var itemTask in taskAffectedList)
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
                result = MessageBox.Show(Resources["restoreTaskAffectedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
            if (result == MessageBoxResult.No)
                return false;

            if (result == MessageBoxResult.Yes && checkFlag)
            {
                foreach (var itemTask in taskAffectedList)
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

        /*private bool ChainCheck()
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
            int count = 0;
            if (brokenTaskList.Count > 0) // bozulacak görevler var
            {
                foreach (var item in brokenTaskList)
                {
                    Console.WriteLine(item.Name);
                }
                var result = MessageBox.Show(Resources["volumeAffectedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No);
                if (result == MessageBoxResult.No)
                {
                    return false;
                }
                else
                {
                    _logger.Information("Kullanıcının onayı doğrultusunda işlemekte olan diğer görevler etkisiz hale getiriliyor.");
                    foreach (var itemTask in brokenTaskList)
                    {
                        if (itemTask.Status == TaskStatusType.Ready || itemTask.Status == TaskStatusType.FirstMissionExpected) // çalışmayan görevlerin etkisiz hale getirilmesine izin vermek
                        {
                            itemTask.EnableDisable = TecnicalTaskStatusType.Broken;

                            if (itemTask.ScheduleId != null && itemTask.ScheduleId != "")
                            {
                                var taskSchedulerManager = _scope.Resolve<ITaskSchedulerManager>();
                                taskSchedulerManager.DeleteJob(itemTask.ScheduleId);
                                itemTask.ScheduleId = "";
                            }

                            foreach (var item in itemTask.StrObje)
                            {
                                try
                                {
                                    _backupService.CleanChain(item);
                                }
                                catch (Exception ex)
                                {
                                    _logger.Error(ex, "Beklenmedik hatadan dolayı {harf} zincir temizleme işlemi gerçekleştirilemedi.", item);
                                    MessageBox.Show(Resources["unexpectedError1MB"].ToString() + $" {item} " + Resources["notChainCleanMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                                    Close();
                                    return false;
                                }
                            }

                            _taskInfoDal.Update(itemTask);
                            count++;
                        }
                    }
                }
            }
            if (count == brokenTaskList.Count)
                return true;
            else
            {
                MessageBox.Show(Resources["runningTaskAffectedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }
        }*/

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
                        if (item.Type == BackupStorageType.Windows)
                        {
                            var totalSize = item.StrCapacity.Split(' ');
                            lblTargetTotalSize.Text = totalSize.First();
                            lblTargetTotalSizeType.Text = totalSize.Last();

                            var freeSize = item.StrFreeSize.Split(' ');
                            lblTargetFreeSize.Text = freeSize.First();
                            lblTargetFreeSizeType.Text = freeSize.Last();

                            var fullSize = item.StrUsedSize.Split(' ');
                            lblTargetFullSize.Text = fullSize.First(); ;
                            lblTargetFullSizeType.Text = fullSize.Last();
                        }
                        else
                        {
                            lblTargetTotalSize.Text = "-";
                            lblTargetTotalSizeType.Text = "GB";

                            lblTargetFreeSize.Text = "-";
                            lblTargetFreeSizeType.Text = "GB";

                            lblTargetFullSize.Text = "-";
                            lblTargetFullSizeType.Text = "GB";
                        }


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

                            var narbulutTotalSize = item.StrCloudCapacity.Split(' ');
                            lblTargetNarbulutTotalSize.Text = narbulutTotalSize.First();
                            lblTargetNarbulutTotalSizeType.Text = narbulutTotalSize.Last();
                            var narbulutFreeSize = item.StrCloudFreeSize.Split(' ');
                            lblTargetNarbulutFreeSize.Text = narbulutFreeSize.First();
                            lblTargetNarbulutFreeSizeType.Text = narbulutFreeSize.Last();
                            var narbulutFullSize = item.StrCloudUsedSize.Split(' ');
                            lblTargetNarbulutFullSize.Text = narbulutFullSize.First(); ;
                            lblTargetNarbulutFullSizeType.Text = narbulutFullSize.Last(); ;

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

            try
            {
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

                _backupStorageInfoList = GetBackupStorages(volumeList, _backupStorageService.BackupStorageInfoList());
                //cbTargetBackupArea.ItemsSource = _backupStorageInfoList;
                GetBackupStorageList(_backupStorageInfoList);
                if (cbTargetBackupArea.Items.Count > 0)
                    cbTargetBackupArea.SelectedIndex = cbTargetBackupArea.Items.Count - 1;
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Beklenmedik hata oluştu.");
                MessageBox.Show(Resources["unexpectedErrorMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }

        }

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
        #endregion

        private void btnDaysTimeDays_Click(object sender, RoutedEventArgs e)
        {
            //ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(true);
            //chooseDays.ShowDialog();
            if (_taskInfo.BackupTaskInfo.Days == null)
            {
                ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(true, _configurationDataDal);
                chooseDays.ShowDialog();
                _taskInfo.BackupTaskInfo.Days = ChooseDayAndMounthsWindow._days;
            }
            else
            {
                // doldurma yap
                ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(true, _taskInfo.BackupTaskInfo.Days, _updateControl, _configurationDataDal);
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
                ChooseDayAndMounthsWindow chooseMounths = new ChooseDayAndMounthsWindow(false, _configurationDataDal);
                chooseMounths.ShowDialog();
                _taskInfo.BackupTaskInfo.Months = ChooseDayAndMounthsWindow._months;
            }
            else
            {
                ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(false, _taskInfo.BackupTaskInfo.Months, _updateControl, _configurationDataDal);
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


        private void GetBackupStorageList(List<BackupStorageInfo> backupStorageInfoList)
        {
            List<BackupStorageInfo> deleteBackupStorageList = new List<BackupStorageInfo>();
            foreach (var item in backupStorageInfoList)
            {
                if (item.Type == BackupStorageType.Windows)
                {
                    if (_taskInfo.StrObje.Contains(item.Path[0]))
                        deleteBackupStorageList.Add(item);
                }
            }

            foreach (var item in deleteBackupStorageList)
            {
                backupStorageInfoList.Remove(item);
            }

            _backupStorageInfoList = backupStorageInfoList;
            cbTargetBackupArea.ItemsSource = _backupStorageInfoList;
        }

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
            if (txtTaskName.Text.Equals("") || cbTargetBackupArea.SelectedIndex == -1)
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
                    if (cbPeriodic.SelectedIndex == -1 && cbPeriodicTime.SelectedIndex == -1)
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
            _taskInfo.BackupTaskId = resultBackupTask.Id;
            _taskInfo.LastWorkingDate = Convert.ToDateTime("01/01/0002");
            var resultTaskInfo = _taskInfoDal.Add(_taskInfo);
            if (resultTaskInfo == null)
            {
                MessageBox.Show(Resources["addFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
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
                MessageBox.Show(Resources["updateFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            //backupTask kaydetme
            _taskInfo.StatusInfo.TaskName = _taskInfo.Name;
            _taskInfo.StatusInfo.SourceObje = _taskInfo.StrObje;
            var resultStatusInfo = _statusInfoDal.Update(_taskInfo.StatusInfo);
            if (resultStatusInfo == null)
            {
                MessageBox.Show(Resources["updateFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
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
                MessageBox.Show(Resources["updateFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return null;
            }

            return resultTaskInfo;
        }

        public List<BackupStorageInfo> GetBackupStorages(List<VolumeInfo> volumeList, List<BackupStorageInfo> backupStorageInfoList)
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
            }

            return backupStorageInfoList;
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

        private void cbPeriodicTime_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (cbPeriodicTime.SelectedIndex == 0) // saat
            {
                List<int> hourList = new List<int> { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };
                cbPeriodic.ItemsSource = hourList;
                cbPeriodic.SelectedIndex = 0;
            }
            else // dakika
            {
                List<int> minuteList = new List<int> { 15, 30, 45, 60, 75, 90, 105, 120 };
                cbPeriodic.ItemsSource = minuteList;
                cbPeriodic.SelectedIndex = 0;
            }
        }

        private void cbFullBackupTimeType_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (cbFullBackupTimeType.SelectedIndex == 0) // gün
            {
                List<int> dayList = new List<int> { 1, 2, 3, 4, 5, 6, 7 };
                cbFullBackupTime.ItemsSource = dayList;
                cbFullBackupTime.SelectedIndex = 0;
            }
            else // hafta
            {
                List<int> weekList = new List<int> { 1, 2, 3, 4 };
                cbFullBackupTime.ItemsSource = weekList;
                cbFullBackupTime.SelectedIndex = 1;
            }
        }
    }
}
