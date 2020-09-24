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
    }
}
