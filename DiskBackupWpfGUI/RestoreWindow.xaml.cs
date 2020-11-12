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
    public partial class RestoreWindow : Window
    {
        public RestoreWindow()
        {
            InitializeComponent();

            dtpSetTime.Value = DateTime.Now + TimeSpan.FromMinutes(5);
        }

        #region Title Bar
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
        #endregion

        #region Next-Back-Ok-Cancel Button
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

            // özet yazımı
            if (RTabControl.SelectedIndex == 2)
            {
                lblTaskName.Text = txtTaskName.Text;

                if (rbStartNow.IsChecked.Value) // hemen çalıştır
                {
                    lblSchedulerTasks.Text = "Hemen Çalıştır";
                }
                else if (rbSetTime.IsChecked.Value) // şu saatte çalıştır
                {
                    lblSchedulerTasks.Text = dtpSetTime.Text;
                }
            }
        }

        private void btnRestoreOk_Click(object sender, RoutedEventArgs e)
        {
            //Kaydedip Silinecek

            if (dtpSetTime.Value <= DateTime.Now + TimeSpan.FromSeconds(10))
            {
                MessageBox.Show("Geçmiş tarih için geri yükleme işlemi gerçekleştirilemez.", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else
            {
                if (rbStartNow.IsChecked.Value) // hemen çalıştır
                {
                    if (txtTaskName.Text.Equals("") || txtTaskDescription.Text.Equals(""))
                    {
                        MessageBox.Show("İlgili alanları lütfen boş geçmeyiniz. Hemen çalıştır", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
                else if (rbSetTime.IsChecked.Value) // zaman belirle
                {
                    if (txtTaskName.Text.Equals("") || txtTaskDescription.Text.Equals("") || dtpSetTime.Value.Equals(""))
                    {
                        MessageBox.Show("İlgili alanları lütfen boş geçmeyiniz. Zaman belirle", "NARBULUT DİYOR Kİ;", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
            }

        }

        private void btnRestoreCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        #endregion

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
