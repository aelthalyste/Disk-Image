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
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for AddBackupArea.xaml
    /// </summary>
    public partial class AddBackupAreaWindow : Window
    {
        //PATH SONUNA TERS SLAS EKLE BATU DA ÖYLE İSTENİYOR
        private bool ShowSettings = false;
        public AddBackupAreaWindow()
        {
            InitializeComponent();
        }

        #region Title Bar
        private void btnABAMin_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void btnABAClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
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

            if (ABATabControl.SelectedIndex == 3)
            {
                if (rbLocalDisc.IsChecked.Value) // yerel disk
                {
                    txtName.Text = txtBackupAreaName.Text;
                    txtTarget.Text = Resources["localDisc"].ToString();
                    txtFolderPath.Text = txtSettingsFolderPath.Text;
                    if (cbBackupToCloud.IsChecked.Value)
                        txtBackupCloud.Text = Resources["active"].ToString();
                    else
                        txtBackupCloud.Text = Resources["inactive"].ToString();
                }
                else if (rbNAS.IsChecked.Value) // nas
                {
                    txtName.Text = txtBackupAreaName.Text;
                    txtTarget.Text = "NAS cihazı";
                    txtFolderPath.Text = txtSettingsNASFolderPath.Text;
                    if (cbBackupToCloud.IsChecked.Value)
                        txtBackupCloud.Text = Resources["active"].ToString();
                    else
                        txtBackupCloud.Text = Resources["inactive"].ToString();
                }
            }
        }

        private void btnBackupAreaOk_Click(object sender, RoutedEventArgs e)
        {
            bool controlFlag = true;
            //kaydet
            if (rbLocalDisc.IsChecked.Value) // yerel disk
            {
                if (txtBackupAreaName.Text.Equals("") || txtBackupAreaDescription.Text.Equals("") || txtSettingsFolderPath.Text.Equals(""))
                {
                    MessageBox.Show("İlgili alanları lütfen boş geçmeyiniz. Yerel Disk");
                    controlFlag = false;
                }
            }
            else if (rbNAS.IsChecked.Value) // nas
            {
                if (txtBackupAreaName.Text.Equals("") || txtBackupAreaDescription.Text.Equals("") || 
                    txtSettingsNASFolderPath.Text.Equals("") || txtSettingsNASDomain.Text.Equals("") ||
                    txtSettingsNASUserName.Text.Equals("") || txtSettingsNASPassword.Password.Equals(""))
                {
                    MessageBox.Show("İlgili alanları lütfen boş geçmeyiniz. NAS");
                    controlFlag = false;
                }
            }

            if (controlFlag)
            {
                //kaydet
                if (rbLocalDisc.IsChecked.Value) // yerel disk
                {
                    BackupStorageInfo backupStorageInfo = new BackupStorageInfo
                    {
                        StorageName = txtBackupAreaName.Text,
                        Description = txtBackupAreaDescription.Text,
                        Path = txtSettingsFolderPath.Text + @"\",
                        IsCloud = cbBackupToCloud.IsChecked.Value,
                    };

                    if (cbBackupToCloud.IsChecked.Value)
                    {
                        backupStorageInfo.Type = BackupStorageType.Hybrit;
                        // bulut işlemleri gelecek (kullanılan alan vs...)
                    }
                    else
                    {
                        backupStorageInfo.Type = BackupStorageType.Windows;
                    }
                }
                else // Nas
                {
                    BackupStorageInfo backupStorageInfo = new BackupStorageInfo
                    {
                        StorageName = txtBackupAreaName.Text,
                        Description = txtBackupAreaDescription.Text,
                        Path = txtSettingsNASFolderPath.Text + @"\",
                        IsCloud = cbBackupToCloud.IsChecked.Value,
                        Domain = txtSettingsNASDomain.Text,
                        Username = txtSettingsNASUserName.Text,
                        Password = txtSettingsNASPassword.Password
                    };

                    if (cbBackupToCloud.IsChecked.Value)
                    {
                        backupStorageInfo.Type = BackupStorageType.Hybrit;
                        // bulut işlemleri gelecek (kullanılan alan vs...)
                    }
                    else
                    {
                        backupStorageInfo.Type = BackupStorageType.NAS;
                    }
                }
            }
        }

        private void btnBackupAreaCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        #endregion

        private void rbLocalDisc_Checked(object sender, RoutedEventArgs e)
        {
            ShowSettings = false;
        }

        private void rbNAS_Checked(object sender, RoutedEventArgs e)
        {
            ShowSettings = true;
        }

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

        private void btnSettingsNASFolder_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new System.Windows.Forms.FolderBrowserDialog();
            System.Windows.Forms.DialogResult result = dialog.ShowDialog();
            txtSettingsNASFolderPath.Text = dialog.SelectedPath;
        }

        private void btnSettingsLocalFolder_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new System.Windows.Forms.FolderBrowserDialog();
            System.Windows.Forms.DialogResult result = dialog.ShowDialog();
            txtSettingsFolderPath.Text = dialog.SelectedPath;
        }

    }
}
