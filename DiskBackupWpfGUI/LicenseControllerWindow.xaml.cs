﻿using DiskBackupWpfGUI.Utils;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
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
        private bool _windowType;
        public bool _validate = false;
        private IConfigHelper _configHelper;

        public LicenseControllerWindow(bool windowType, IConfigHelper configHelper)
        {
            InitializeComponent();

            _windowType = windowType;
            _configHelper = configHelper;

            SetApplicationLanguage(_configHelper.GetConfig("lang"));

            rbDemo.Checked += rbDemo_Checked;
            rbLicense.Checked += rbLicense_Checked;

            if (windowType)
            {
                rbDemo.Visibility = Visibility.Collapsed;
                rbLicense.IsChecked = true;
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
            txtLicenseKey.IsEnabled = false;
        }

        private void rbLicense_Checked(object sender, RoutedEventArgs e)
        {
            txtLicenseKey.IsEnabled = true;
        }

        private void btnSave_Click(object sender, RoutedEventArgs e)
        {
            var key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\NarDiskBackup", true);

            if (!_windowType)
            {
                if (key == null)
                {
                    key = Registry.CurrentUser.CreateSubKey("SOFTWARE\\NarDiskBackup");
                }
            }

            if (rbDemo.IsChecked == true)
            {
                key.SetValue("UploadDate", DateTime.Now);
                key.SetValue("ExpireDate", DateTime.Now + TimeSpan.FromDays(31));
                key.SetValue("LastDate", DateTime.Now);
                key.SetValue("Type", 1505);
                _validate = true;
                Close();
            }
            else // lisans seçili
            {
                if (DecryptLicenseKey("D*G-KaPdSgVkYp3s6v8y/B?E(H+MbQeT", txtLicenseKey.Text).Equals("fail"))
                {
                    MessageBox.Show(Resources["LicenseKeyFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
                else
                {
                    key.SetValue("UploadDate", DateTime.Now);
                    key.SetValue("ExpireDate", "");
                    key.SetValue("Type", 2606);
                    key.SetValue("License", txtLicenseKey.Text);
                    _validate = true;
                    Close();
                }
            }
        }

        private string DecryptLicenseKey(string key, string cipherLicenseKey)
        {
            if (cipherLicenseKey == null || cipherLicenseKey == "" || cipherLicenseKey.Contains(' '))
                return "fail";

            try
            {
                var iv = Convert.FromBase64String("EEXkANPr+5R9q+XyG7jR5w==");
                byte[] buffer = Convert.FromBase64String(cipherLicenseKey);

                using (Aes aes = Aes.Create())
                {
                    aes.Key = Encoding.UTF8.GetBytes(key);
                    aes.IV = iv;
                    ICryptoTransform decryptor = aes.CreateDecryptor(aes.Key, aes.IV);

                    using (MemoryStream memoryStream = new MemoryStream(buffer))
                    {
                        using (CryptoStream cryptoStream = new CryptoStream((Stream)memoryStream, decryptor, CryptoStreamMode.Read))
                        {
                            using (StreamReader streamReader = new StreamReader((Stream)cryptoStream))
                            {
                                return streamReader.ReadToEnd();
                            }
                        }
                    }
                }
            }
            catch (Exception)
            {
                return "fail";
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
