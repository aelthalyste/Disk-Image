using System;
using System.Collections.Generic;
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
using System.Windows.Shapes;

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for Restore.xaml
    /// </summary>
    public partial class Restore : Window
    {
        public Restore()
        {
            InitializeComponent();
            //minimum date kontrol etme handle edemedik
            dtpSetTime.Minimum = DateTime.Now;
            dtpSetTime.Value = DateTime.Now;

        }

        private void btnRestoreBack_Click(object sender, RoutedEventArgs e)
        {
            if (RTabControl.SelectedIndex != 0)
            {
                RTabControl.SelectedIndex -= 1;
            }
        }

        private void btnRestoreNext_Click(object sender, RoutedEventArgs e)
        {
            if (RTabControl.SelectedIndex != 3)
            {
                RTabControl.SelectedIndex += 1;
            }
        }

        private void btnRestoreOk_Click(object sender, RoutedEventArgs e)
        {
            //Kaydedip Silinecek
        }

        private void btnRestoreCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void btnRestoreClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void btnRestoreMin_Click(object sender, RoutedEventArgs e)
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

        private void RTabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (RTabControl.SelectedIndex == 0)
            {
                lblTabHeader.Text = Resources["name"].ToString();
                lblTabContent.Text = Resources["RNameContent"].ToString();
            }
            else if (RTabControl.SelectedIndex == 1)
            {
                lblTabHeader.Text = Resources["scheduler"].ToString();
                lblTabContent.Text = Resources["RSchedulerContent"].ToString();
            }
            else
            {
                lblTabHeader.Text = Resources["summary"].ToString();
                lblTabContent.Text = Resources["RSummaryContent"].ToString();
            }
        }

        private void chbAutoRun_Checked(object sender, RoutedEventArgs e)
        {
            stackAutoRun.IsEnabled = true;
        }

        private void chbAutoRun_Unchecked(object sender, RoutedEventArgs e)
        {
            stackAutoRun.IsEnabled = false;
            rbSetTime.IsChecked = true;
            rbSetTime.IsChecked = false;
        }

        private void rbSetTime_Checked(object sender, RoutedEventArgs e)
        {
            stackSetTime.IsEnabled = true;
        }

        private void rbSetTime_Unchecked(object sender, RoutedEventArgs e)
        {
            stackSetTime.IsEnabled = false;
        }
    }
}
