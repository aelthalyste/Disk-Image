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
    /// Interaction logic for AddBackupArea.xaml
    /// </summary>
    public partial class AddBackupArea : Window
    {
        private bool ShowSettings = false;
        public AddBackupArea()
        {
            InitializeComponent();
        }

        #region titleBar
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
        #endregion

        private void ABATabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ABATabControl.SelectedIndex == 0)
            {
                lblTabHeader.Text = Resources["name"].ToString();
                lblTabContent.Text = Resources["ABANameContent"].ToString();
            }
            else if (ABATabControl.SelectedIndex == 1)
            {
                lblTabHeader.Text = Resources["target"].ToString();
                lblTabContent.Text = Resources["ABATargetContent"].ToString();
            }
            else if (ABATabControl.SelectedIndex == 2)
            {
                lblTabHeader.Text = Resources["settings"].ToString();
                lblTabContent.Text = Resources["ABASettingsContent"].ToString();
                if (ShowSettings == false)
                {
                    stackLocalDisc.Visibility = Visibility.Visible;
                    stackNAS.Visibility = Visibility.Hidden;
                }
                else
                {
                    stackNAS.Visibility = Visibility.Visible;
                    stackLocalDisc.Visibility = Visibility.Hidden;
                }
            }
            else
            {
                lblTabHeader.Text = Resources["summary"].ToString();
                lblTabContent.Text = Resources["ABASummaryContent"].ToString();
            }
        }

        #region Next-Back-Ok-Cancel Button
        private void btnBackupAreaBack_Click(object sender, RoutedEventArgs e)
        {
            if (ABATabControl.SelectedIndex != 0)
            {
                ABATabControl.SelectedIndex -= 1;
            }
        }

        private void btnBackupAreaNext_Click(object sender, RoutedEventArgs e)
        {
            if (ABATabControl.SelectedIndex != 3)
            {
                ABATabControl.SelectedIndex += 1;
            }
        }

        private void btnBackupAreaOk_Click(object sender, RoutedEventArgs e)
        {
            //kaydet
        }

        private void btnBackupAreaCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        #endregion

        private void btnSettingsNASFolder_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new System.Windows.Forms.FolderBrowserDialog();
            System.Windows.Forms.DialogResult result = dialog.ShowDialog();
            txtSettingsNASFolderPath.Text = dialog.SelectedPath;
        }

        private void rbLocalDisc_Checked(object sender, RoutedEventArgs e)
        {
            ShowSettings = false;
        }

        private void rbNAS_Checked(object sender, RoutedEventArgs e)
        {
            ShowSettings = true;
        }

        private void btnSettingsFolder_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new System.Windows.Forms.FolderBrowserDialog();
            System.Windows.Forms.DialogResult result = dialog.ShowDialog();
            txtSettingsNASFolderPath.Text = dialog.SelectedPath;
        }

        private void btnABAMin_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void btnABAClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
