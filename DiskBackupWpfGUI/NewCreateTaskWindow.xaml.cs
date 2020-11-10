using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.Entities.Concrete;
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

        private List<BackupStorageInfo> _backupStorageInfoList = new List<BackupStorageInfo>();

        private readonly Func<AddBackupAreaWindow> _createAddBackupWindow;

        public NewCreateTaskWindow(List<BackupStorageInfo> backupStorageInfoList, IBackupService backupService, IBackupStorageService backupStorageService, Func<AddBackupAreaWindow> createAddBackupWindow)
        {
            InitializeComponent();

            _backupStorageInfoList = backupStorageInfoList;

            cbTargetBackupArea.ItemsSource = _backupStorageInfoList;
            _backupService = backupService;
            _backupStorageService = backupStorageService;
            _createAddBackupWindow = createAddBackupWindow;
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
            //kaydet
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
            ChooseDayAndMounthsWindow chooseDays = new ChooseDayAndMounthsWindow(true);
            chooseDays.ShowDialog();
        }

        private void btnWeeklyTimeWeek_Click(object sender, RoutedEventArgs e)
        {
            ChooseDayAndMounthsWindow chooseMounths = new ChooseDayAndMounthsWindow(false);
            chooseMounths.ShowDialog();
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
            }
            else if (NCTTabControl.SelectedIndex == 1)
            {
                lblTabHeader.Text = Resources["BackupType"].ToString();
                lblTabContent.Text = Resources["NCTBackupTypeContent"].ToString();
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
            }
            else
            {
                lblTabHeader.Text = Resources["summary"].ToString();
                lblTabContent.Text = Resources["NCTSummaryContent"].ToString();
            }
        }

    }
}
