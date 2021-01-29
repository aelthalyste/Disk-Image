using LicenseKeyGenerator.DataAccess.Abstract;
using LicenseKeyGenerator.DataAccess.Concrete.EntityFramework;
using LicenseKeyGenerator.Entities;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
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
        private ILicenseDal _licenseDal;
        public MainWindow()
        {
            InitializeComponent();
            _licenseDal = new EfLicenseDal();
        }

        private void licenseKeyTabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            //if (licenseKeyTabControl.SelectedIndex == 2)
            //{
            //    txtLicenses.Document.Blocks.Clear();
            //    StreamReader streamReader = new StreamReader(licenceKeyFile);
            //    var text = streamReader.ReadToEnd();
            //    streamReader.Close();
            //    txtLicenses.AppendText(text);
            //}
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

        private void btnNormal_Click(object sender, RoutedEventArgs e)
        {
            if (WindowState == WindowState.Maximized)
                WindowState = WindowState.Normal;
            else
                WindowState = WindowState.Maximized;
        }

        #endregion

        private void btnEncrypt_Click(object sender, RoutedEventArgs e)
        {
            if (IsNullCheck())
            {
                License license = new License();
                if (rbServer.IsChecked.Value)
                    license.LicenseVersion = VersionType.Server;
                else if (rbWorkStation.IsChecked.Value)
                    license.LicenseVersion = VersionType.Workstation;
                else if (rbSBS.IsChecked.Value)
                    license.LicenseVersion = VersionType.SBS;

                license.DealerName = txtDealerName.Text;
                license.CustomerName = txtCustomerName.Text;
                license.AuthorizedPerson = txtAuthorizedPerson.Text;
                license.CreatedDate = DateTime.Now;
                license.SupportEndDate = license.CreatedDate.AddDays(Convert.ToDouble(txtEndDate.Text));
                license.UniqKey = UniqKeyGenerator();

                var str = license.DealerName + "_" + license.CustomerName + "_" + license.AuthorizedPerson + "_" + license.SupportEndDate + "_" + license.LicenseVersion.ToString() + "_" + license.UniqKey;
                license.Key = EncryptString(key, str);
                txtLicenceKey.Text = license.Key;
                txtVerificationKey.Text = license.UniqKey;
                _licenseDal.Add(license);
                MessageBox.Show("Lisans Anahtarı Oluşturuldu");
            }
            else
            {
                MessageBox.Show("LÜTFEN BOŞ ALANLARI DOLDURUNUZ");
            }
        }

        private bool IsNullCheck()
        {
            if (txtDealerName.Text == "" || txtCustomerName.Text == "" || txtAuthorizedPerson.Text == "" || txtEndDate.Text == "")
                return false;
            return true;
        }

        public string RandomString()
        {
            const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            return new string(Enumerable.Repeat(chars, 10)
              .Select(s => s[random.Next(s.Length)]).ToArray());
        }

        public string UniqKeyGenerator()
        {
            string uniqKey = "";
            while (true)
            {
                uniqKey = RandomString();
                var licenseList = _licenseDal.Get(x => x.UniqKey == uniqKey);
                if (licenseList == null)
                    return uniqKey;
            }
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

        private void btnExportFile_Click(object sender, RoutedEventArgs e)
        {

        }

        private void btnShowMore_Click(object sender, RoutedEventArgs e)
        {
            KeyInfoWindow keyInfoWindow = new KeyInfoWindow();
            keyInfoWindow.Show();
        }

        private void txtEndDate_TextChanged(object sender, TextChangedEventArgs e)
        {
            txtEndDate.Text = Regex.Replace(txtEndDate.Text, "[^0-9]+", "");
        }

        private void btnClear_Click(object sender, RoutedEventArgs e)
        {
            //List<string> konuşma = new List<string>();
            //konuşma.Add("Eyüp: Ebru Temizlenmesi istiyorlarmış \nEbru:Offff!");
            //konuşma.Add("Ebru: Eyüp Temizlenmesi istiyorlarmış \nEyüp:Offff!");
            //konuşma.Add("Ebru: Eyüp sen temizler misin \nEyüp:Offff!");
            //konuşma.Add("Ebru: Eyüp temizlenmesi gerekliymiş ben sana demiştim şuraya buton koyalım diye  \nEyüp:Offff!");
            //Random random = new Random(); 
            //MessageBox.Show(konuşma[Convert.ToInt32(random.Next(0, konuşma.Count - 1))]);
            txtDealerName.Text = "";
            txtCustomerName.Text = "";
            txtAuthorizedPerson.Text = "" ;
            txtEndDate.Text = "";
            txtVerificationKey.Text = "";
            txtLicenceKey.Text = "";
            rbWorkStation.IsChecked = true;
        }
    }
}
