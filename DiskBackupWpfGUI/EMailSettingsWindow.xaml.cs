using DiskBackup.Communication;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
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
    /// Interaction logic for EMailSettingsWindow.xaml
    /// </summary>
    public partial class EMailSettingsWindow : Window
    {
        private readonly IConfigurationDataDal _configurationDataDal;
        private IEmailInfoDal _emailInfoDal;
        private IEMailOperations _eMailOperations;

        public EMailSettingsWindow(IConfigurationDataDal configurationDataDal, IEmailInfoDal emailInfoDal, IEMailOperations eMailOperations)
        {
            InitializeComponent();
            _emailInfoDal = emailInfoDal;
            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            listBoxEmailAddresses.ItemsSource = _emailInfoDal.GetList();
            _eMailOperations = eMailOperations;
        }

        #region Title Bar
        private void btnEMSClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void btnEMSMin_Click(object sender, RoutedEventArgs e)
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


        private void btnEmailAdd_Click(object sender, RoutedEventArgs e)
        {
            AddEmail();
        }

        private void btnEmailDelete_Click(object sender, RoutedEventArgs e)
        {
            if (listBoxEmailAddresses.SelectedIndex > -1)
            {
                _emailInfoDal.Delete(_emailInfoDal.Get(x => x.EmailAddress == ((EmailInfo)listBoxEmailAddresses.SelectedItem).EmailAddress));
                listBoxEmailAddresses.ItemsSource = _emailInfoDal.GetList();
            }
        }

        private void AddEmail()
        {
            if (Regex.IsMatch(txtNewEmailAddress.Text, @"^[a-zA-Z][\w\.-]*[a-zA-Z0-9]@[a-zA-Z0-9][\w\.-]*[a-zA-Z0-9]\.[a-zA-Z][a-zA-Z\.]*[a-zA-Z]$"))
            {
                var resultEmailInfo = _emailInfoDal.Add(new EmailInfo { EmailAddress = txtNewEmailAddress.Text });
                if (resultEmailInfo != null)
                {
                    txtNewEmailAddress.Text = "";
                    listBoxEmailAddresses.ItemsSource = _emailInfoDal.GetList();
                }
                else
                {
                    MessageBox.Show(Resources["addFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            else
            {
                MessageBox.Show(Resources["EmailFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void txtNewEmailAddress_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                AddEmail();
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

        private void btnTesting_Click(object sender, RoutedEventArgs e)
        {
            _eMailOperations.SendTestEMail();
        }
    }
}
