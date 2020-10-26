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
        public NewCreateTaskWindow()
        {
            InitializeComponent();
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

        private void btnTargetAdd_Click(object sender, RoutedEventArgs e)
        {
            AddBackupAreaWindow addBackupArea = new AddBackupAreaWindow();
            addBackupArea.ShowDialog();
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
