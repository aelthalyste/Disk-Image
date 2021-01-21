﻿using System;
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
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace LicenseKeyGenerator
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public string key = "D*G-KaPdSgVkYp3s6v8y/B?E(H+MbQeT";
        private Random random = new Random();
        private string licenceKeyFile = "Lisans Anahtarları.txt";

        public MainWindow()
        {
            InitializeComponent();
        }

        private void licenseKeyTabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (licenseKeyTabControl.SelectedIndex == 2)
            {
                txtLicenses.Document.Blocks.Clear();
                StreamReader streamReader = new StreamReader(licenceKeyFile);
                var text = streamReader.ReadToEnd();
                streamReader.Close();
                txtLicenses.AppendText(text);
            }
        }

        #region Title Bar

        private void btnMin_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void btnClose_Click(object sender, RoutedEventArgs e)
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

        private void btnEncryptt_Click(object sender, RoutedEventArgs e)
        {
            string str = "";
            DateTime dateTime = DateTime.Now;
            str = RandomString(8) + "_" + dateTime + "_" + RandomString(8);
            var encryptedString = EncryptString(key, str);
            txtResult.Text = encryptedString;
            txtDecyrpt.Text = DecryptString(key, encryptedString);
            StreamWriter sw = new StreamWriter(licenceKeyFile, true);
            sw.WriteLine(dateTime + "\t" + encryptedString);
            sw.Flush();
            sw.Close();
        }

        public string RandomString(int length)
        {
            const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
            return new string(Enumerable.Repeat(chars, length)
              .Select(s => s[random.Next(s.Length)]).ToArray());
        }

        public string DecryptString(string key, string cipherText)
        {
            try
            {
                var iv = Convert.FromBase64String("EEXkANPr+5R9q+XyG7jR5w==");
                byte[] buffer = Convert.FromBase64String(cipherText);

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

        public string EncryptString(string key, string plainText)
        {
            try
            {
                var iv = Convert.FromBase64String("EEXkANPr+5R9q+XyG7jR5w==");
                byte[] array;

                using (Aes aes = Aes.Create())
                {
                    aes.Key = Encoding.UTF8.GetBytes(key);
                    aes.IV = iv;
                    ICryptoTransform encryptor = aes.CreateEncryptor(aes.Key, aes.IV);

                    using (MemoryStream memoryStream = new MemoryStream())
                    {
                        using (CryptoStream cryptoStream = new CryptoStream((Stream)memoryStream, encryptor, CryptoStreamMode.Write))
                        {
                            using (StreamWriter streamWriter = new StreamWriter((Stream)cryptoStream))
                            {
                                streamWriter.Write(plainText);
                            }

                            array = memoryStream.ToArray();
                            return Convert.ToBase64String(array);
                        }
                    }
                }
            }
            catch (Exception)
            {
                return "fail";
            }
        }

        private void btnDecrypt_Click(object sender, RoutedEventArgs e)
        {
            txtDecyrpt2.Text = DecryptString(key, txtEncryptText2.Text);
        }
    }
}
