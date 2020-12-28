using DiskBackup.Business.Abstract;
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
    /// Interaction logic for ValidateNAS.xaml
    /// </summary>
    public partial class ValidateNASWindow : Window
    {
        private IBackupStorageService _backupStorageService;
        private BackupStorageInfo _backupStorageInfo;

        public ValidateNASWindow(IBackupStorageService backupStorageService, BackupStorageInfo backupStorageInfo)
        {
            InitializeComponent();
            _backupStorageService = backupStorageService;
            _backupStorageInfo = backupStorageInfo;

            txtValidateNASFolderPath.Text = backupStorageInfo.Path.Substring(0, backupStorageInfo.Path.Length - 1);
            txtValidateNASDomain.Text = backupStorageInfo.Domain;
            txtValidateNASUserName.Text = backupStorageInfo.Username;
        }

        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }

        private void btnValidateNAS_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (_backupStorageService.ValidateNasConnection(_backupStorageInfo.Path.Substring(0, _backupStorageInfo.Path.Length - 1), _backupStorageInfo.Username, txtValidateNASPassword.Password, _backupStorageInfo.Domain))
                {
                    //doğrulama başarılı
                    imgValidateConnectionFalse.Visibility = Visibility.Collapsed;
                    imgValidateConnectionTrue.Visibility = Visibility.Visible;

                    // silme işlemi 
                }
                else
                {
                    //başarısız
                    imgValidateConnectionFalse.Visibility = Visibility.Visible;
                    imgValidateConnectionTrue.Visibility = Visibility.Collapsed;
                }
            }
            catch
            {
            }
        }

        private void btnValidateNASClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
