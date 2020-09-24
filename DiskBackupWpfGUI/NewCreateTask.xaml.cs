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
    public partial class NewCreateTask : Window
    {
        public NewCreateTask()
        {
            InitializeComponent();
        }

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

        private void checkTimeFailDesc_Unchecked(object sender, RoutedEventArgs e)
        {
            stackTimeFailDesc.IsEnabled = false;
        }

        private void checkTimeFailDesc_Checked(object sender, RoutedEventArgs e)
        {
            stackTimeFailDesc.IsEnabled = true;
        }

        private void btnTimeFailDescUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtTimeFailDesc.Text);
            if(count != 999)
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

        private void btnDaysTimeUp_Click(object sender, RoutedEventArgs e)
        {
            //string[] parcalar = txtDaysTime.Text.Split(',');
            //int p1 = Convert.ToInt32(parcalar[0]);
            //int p2 = Convert.ToInt32(parcalar[1]);
            //double time = p1 + (p2 / 100);
            //time += 0.01;
            //txtDaysTime.Text = time.ToString();
        }

        private void NCTTabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (NCTTabControl.SelectedIndex == 0)
            {
                lblTabHeader.Text = Resources["name"].ToString();
                lblTabContent.Text = "Yedekleme profili için bir isim ve açıklama giriniz";
            }
            else if (NCTTabControl.SelectedIndex == 1)
            {
                lblTabHeader.Text = Resources["BackupType"].ToString();
                lblTabContent.Text = "Yedekleme istediğiniz türü seçin";
            }
            else if (NCTTabControl.SelectedIndex == 2)
            {
                lblTabHeader.Text = Resources["target"].ToString();
                lblTabContent.Text = "Yedeklemek istediğiniz veri tabanı türünü seçiniz erişimi doğrulayınız";
            }
            else if (NCTTabControl.SelectedIndex == 3)
            {
                lblTabHeader.Text = Resources["scheduler"].ToString();
                lblTabContent.Text = "Yedeklemek istediğiniz veri tabanı türünü seçiniz erişimi doğrulayınız";
            }
            else
            {
                lblTabHeader.Text = Resources["abstarct"].ToString();
                lblTabContent.Text = "Yedeklemek istediğiniz veri tabanı türünü seçiniz erişimi doğrulayınız";
            }
        }

        private void btnCreateTaskBack_Click(object sender, RoutedEventArgs e)
        {
            if(NCTTabControl.SelectedIndex != 0)
            {
                NCTTabControl.SelectedIndex -= 1;
            }
        }

        private void btnCreateTaskNext_Click(object sender, RoutedEventArgs e)
        {
            if (NCTTabControl.SelectedIndex != 4)
            {
                NCTTabControl.SelectedIndex += 1;
            }
        }

        private void btnCreateTaskOk_Click(object sender, RoutedEventArgs e)
        {
            //kaydet
        }

        private void btnCreateTaskCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

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
    }
}
