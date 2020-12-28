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
        private IBackupService _backupService;

        private BackupStorageInfo _backupStorageInfo;
        private BackupInfo _backupInfo;

        public bool _validate = false;

        public ValidateNASWindow(IBackupStorageService backupStorageService, IBackupService backupService, BackupInfo backupInfo)
        {
            InitializeComponent();
            _backupStorageService = backupStorageService;
            _backupService = backupService;
            _backupInfo = backupInfo;
            _backupStorageInfo = backupInfo.BackupStorageInfo;

            txtValidateNASFolderPath.Text = backupInfo.BackupStorageInfo.Path.Substring(0, backupInfo.BackupStorageInfo.Path.Length - 1);
            txtValidateNASDomain.Text = backupInfo.BackupStorageInfo.Domain;
            txtValidateNASUserName.Text = backupInfo.BackupStorageInfo.Username;
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
                    _validate = true;
                    // silme işlemleri                  
                    var result2 = _backupService.BackupFileDelete(_backupInfo);
                    if (result2 == 5)
                        MessageBox.Show("Silme başarılı");
                    else
                        MessageBox.Show("Silme başarısız, " + result2);

                    Close();
                }
                else
                {
                    //başarısız
                    imgValidateConnectionFalse.Visibility = Visibility.Visible;
                    imgValidateConnectionTrue.Visibility = Visibility.Collapsed;
                    _validate = false;
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
