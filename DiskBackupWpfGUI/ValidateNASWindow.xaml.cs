using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackupWpfGUI.Utils;
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
        private readonly IConfigurationDataDal _configurationDataDal;

        private BackupStorageInfo _backupStorageInfo;
        private BackupInfo _backupInfo;

        public bool _validate = false;

        public ValidateNASWindow(IBackupStorageService backupStorageService, IBackupService backupService, BackupInfo backupInfo, IConfigurationDataDal configurationDataDal)
        {
            InitializeComponent();
            _backupStorageService = backupStorageService;
            _backupService = backupService;
            _backupInfo = backupInfo;
            _backupStorageInfo = backupInfo.BackupStorageInfo;

            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

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
                    var result = _backupService.BackupFileDelete(_backupInfo);
                    if (result == 0)
                        MessageBox.Show( Resources["notConnectNASMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    else if (result == 1) 
                        MessageBox.Show(Resources["deleteFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);

                    Console.WriteLine("Validate sildim");

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
                //başarısız
                imgValidateConnectionFalse.Visibility = Visibility.Visible;
                imgValidateConnectionTrue.Visibility = Visibility.Collapsed;
                _validate = false;
            }
        }

        private void btnValidateNASClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        public void SetApplicationLanguage(string option)
        {
            ResourceDictionary dict = new ResourceDictionary();

            switch (option)
            {
                case "tr":
                    dict.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);
                    break;
                case "en":
                    dict.Source = new Uri("..\\Resources\\Lang\\string_eng.xaml", UriKind.Relative);
                    break;
                default:
                    dict.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);
                    break;
            }
            Resources.MergedDictionaries.Add(dict);
        }
    }
}
