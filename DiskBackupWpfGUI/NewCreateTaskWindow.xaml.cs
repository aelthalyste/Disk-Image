using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
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

        private IBackupTaskDal _backupTaskDal;
        private IStatusInfoDal _statusInfoDal;
        private ITaskInfoDal _taskInfoDal;
        private ITaskSchedulerManager _schedulerManager;

        private readonly ILifetimeScope _scope;

        private List<BackupStorageInfo> _backupStorageInfoList = new List<BackupStorageInfo>();
        private List<VolumeInfo> _volumeInfoList = new List<VolumeInfo>();

        private TaskInfo _taskInfo = new TaskInfo();

        private readonly Func<AddBackupAreaWindow> _createAddBackupWindow;

        public NewCreateTaskWindow(List<BackupStorageInfo> backupStorageInfoList, IBackupService backupService, IBackupStorageService backupStorageService,
            Func<AddBackupAreaWindow> createAddBackupWindow, List<VolumeInfo> volumeInfoList, IBackupTaskDal backupTaskDal, IStatusInfoDal statusInfoDal, 
            ITaskInfoDal taskInfoDal, ITaskSchedulerManager schedulerManager, ILifetimeScope scope)
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
                        _taskInfo.BackupTaskInfo.Days = (cbWeeklyTimeDays.SelectedIndex+1).ToString();
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

                //veritabanı işlemleri
                TaskInfo resultTaskInfo = SaveToDatabase();

                if (resultTaskInfo != null)
                {
                    MessageBox.Show("Ekleme işlemi başarılı");
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

                }

                Close();

                // task status açılması
                //resultTaskInfo.StatusInfo = _statusInfoDal.Get(x => x.Id == resultTaskInfo.StatusInfoId);
                //resultTaskInfo.BackupTaskInfo = _backupTaskDal.Get(x => x.Id == resultTaskInfo.BackupTaskId);
                //StatusesWindow backupStatus = _scope.Resolve<StatusesWindow>(new NamedParameter("chooseFlag", 0), new NamedParameter("statusInfo", resultTaskInfo.StatusInfo));
                //backupStatus.Show();
            }

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
                    _taskInfo.BackupTaskInfo.Type = BackupTypes.Diff;
                }
                else if (rbBTIncremental.IsChecked.Value) // inc
                {
                    lblBackupType.Text = Resources["inc"].ToString();
                    _taskInfo.BackupTaskInfo.Type = BackupTypes.Inc;
                }
                else if (rbBTFull.IsChecked.Value) // full
                {
                    lblBackupType.Text = Resources["full"].ToString();
                    _taskInfo.BackupTaskInfo.Type = BackupTypes.Full;
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
                ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(true, _taskInfo.BackupTaskInfo.Days);
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
                ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(false, _taskInfo.BackupTaskInfo.Months);
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
