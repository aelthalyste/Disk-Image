using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackupWpfGUI.Utils;
using Serilog;
using System;
using System.Collections.Generic;
using System.IO;
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
    /// Interaction logic for LicenseControllerWindow.xaml
    /// </summary>
    public partial class LicenseControllerWindow : Window
    {
        public bool _validate = false;

        private ILicenseService _licenseService;
        private readonly IConfigurationDataDal _configurationDataDal;
        private readonly ILogger _logger;

        public LicenseControllerWindow(bool windowType, ILogger logger, IConfigurationDataDal configurationDataDal, ILicenseService licenseService)
        {
            InitializeComponent();

            _licenseService = licenseService;
            _configurationDataDal = configurationDataDal;
            _logger = logger.ForContext<LicenseControllerWindow>();

            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            rbDemo.Checked += rbDemo_Checked;
            rbLicense.Checked += rbLicense_Checked;

            if (windowType)
            {
                rbDemo.Visibility = Visibility.Collapsed;
                txtDemoCustomerName.Visibility = Visibility.Collapsed;
                rbLicense.IsChecked = true;
                Height = 200;
            }
        }

        #region Title Bar
        private void btnChooseDayAndMounthsClose_Click(object sender, RoutedEventArgs e)
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

        private void rbDemo_Checked(object sender, RoutedEventArgs e)
        {
            stackLicense.IsEnabled = false;
            btnAddLicenseFile.IsEnabled = false;
            txtDemoCustomerName.IsEnabled = true;
        }

        private void rbLicense_Checked(object sender, RoutedEventArgs e)
        {
            stackLicense.IsEnabled = true;
            btnAddLicenseFile.IsEnabled = true;
            txtDemoCustomerName.IsEnabled = false;
        }

        private void btnSave_Click(object sender, RoutedEventArgs e)
        {
            if (rbDemo.IsChecked == true)
            {
                if (txtDemoCustomerName.Text != "")
                {
                    _logger.Information("Demo lisans aktifleştirildi.");
                    _licenseService.SetDemoFile(txtDemoCustomerName.Text);
                    _validate = true;
                    Close();
                }
                else
                    MessageBox.Show(Resources["notNullMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else // lisans seçili
            {
                if (txtLicenseKey.Text == null || txtLicenseKey.Text == "" || txtLicenseKey.Text.Contains(' '))
                {
                    MessageBox.Show(Resources["LicenseKeyFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
                else
                {
                    var resultLicense = _licenseService.ValidateLicenseKey(txtLicenseKey.Text);
                    ValidateLicenseKeyController(resultLicense);
                }
            }
        }

        private void ValidateLicenseKeyController(string resultLicense)
        {
            if (resultLicense.Equals("fail"))
            {
                MessageBox.Show(Resources["LicenseKeyFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else if (resultLicense.Equals("failOS"))
            {
                MessageBox.Show(Resources["LicenseKeyOSFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
            else
            {
                // lisans key etkinleştirildi
                _logger.Information("Lisans aktifleştirildi.Lisans Anahtarı: " + txtLicenseKey.Text);
                _validate = true;
                Close();
            }
        }

        private void btnAddLicenseFile_Click(object sender, RoutedEventArgs e)
        {
            using (var dialog = new System.Windows.Forms.OpenFileDialog())
            {
                dialog.Filter = "Narbulut Key Dosyası |*.nbkey";
                dialog.ValidateNames = false;
                dialog.CheckFileExists = false;
                dialog.CheckPathExists = true;
                dialog.Multiselect = false;

                var result = dialog.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    string nbkeyPath = dialog.FileName;
                    StreamReader sr = new StreamReader(nbkeyPath);
                    var licenseKeyFile = sr.ReadToEnd();
                    var resultLicense = _licenseService.ValidateLicenseKey(licenseKeyFile); //yukarı kopyala
                    ValidateLicenseKeyController(resultLicense);
                }
            }
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
