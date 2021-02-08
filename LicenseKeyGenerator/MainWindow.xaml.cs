using LicenseKeyGenerator.DataAccess.Abstract;
using LicenseKeyGenerator.DataAccess.Concrete.EntityFramework;
using LicenseKeyGenerator.Entities;
using LicenseKeyGenerator.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
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
using License = LicenseKeyGenerator.Entities.License; //Column sort işlemini yapmak için gerekli olan "using System.ComponentModel;" eklenince ComponentModel'in altında da license sınıfı olduğu için eklememiz gerekti 

namespace LicenseKeyGenerator
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public string key = "D*G-KaPdSgVkYp3s6v8y/B?E(H+MbQeT";
        private Random random = new Random();
        private ILicenseDal _licenseDal;

        public MainWindow()
        {
            InitializeComponent();
            _licenseDal = new EfLicenseDal();
            RefreshLicenses();
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
            if (e.ClickCount >= 2)
            {
                if (WindowState == WindowState.Maximized)
                    WindowState = WindowState.Normal;
                else
                    WindowState = WindowState.Maximized;
            }

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

        #region Key Oluşturma

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

                var str = license.DealerName + "_" + license.CustomerName + "_" + license.AuthorizedPerson + "_" + license.CreatedDate + "_" + license.SupportEndDate + "_" + license.LicenseVersion.ToString() + "_" + license.UniqKey;
                license.Key = EncryptString(key, str);
                txtLicenceKey.Text = license.Key;
                txtVerificationKey.Text = license.UniqKey;
                _licenseDal.Add(license);
                RefreshLicenses();
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

        private void btnExportFile_Click(object sender, RoutedEventArgs e)
        {
            using (var dialog = new System.Windows.Forms.FolderBrowserDialog())
            {
                var result = dialog.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    ExportFile(dialog.SelectedPath, txtVerificationKey.Text, txtLicenceKey.Text);
                }
            }
        }

        private void ExportFile(string path, string uniqKey, string key)
        {
            string filePath = path + @"\" + uniqKey + ".nbkey";
            FileStream fileStream = new FileStream(filePath, FileMode.OpenOrCreate, FileAccess.Write);
            StreamWriter streamWriter = new StreamWriter(fileStream);
            streamWriter.Write(key);
            streamWriter.Flush();
            streamWriter.Close();
            fileStream.Close();
        }


        private void txtEndDate_TextChanged(object sender, TextChangedEventArgs e)
        {
            txtEndDate.Text = Regex.Replace(txtEndDate.Text, "[^0-9]+", "");
            if (txtDealerName.Text == "" || txtCustomerName.Text == "" || txtAuthorizedPerson.Text == "" || txtEndDate.Text == "")
                btnEncrypt.IsEnabled = false;
            else
                btnEncrypt.IsEnabled = true;
        }

        private void btnClear_Click(object sender, RoutedEventArgs e)
        {
            List<string> konuşma = new List<string>();
            konuşma.Add("Eyüp: Ebru Temizlenmesi istiyorlarmış \nEbru: Hadi yapalım şu işi!");
            konuşma.Add("Ebru: Eyüp Temizlenmesi istiyorlarmış \nEyüp: Hadi yapalım şu işi!");
            konuşma.Add("Ebru: Eyüp sen temizler misin \nEyüp: Offff!");
            konuşma.Add("Ebru: Eyüp temizlenmesi gerekliymiş ben sana demiştim şuraya buton koyalım diye  \nEyüp: Offff!");
            Random random = new Random();
            MessageBox.Show(konuşma[Convert.ToInt32(random.Next(0, konuşma.Count))]);
            txtDealerName.Text = "";
            txtCustomerName.Text = "";
            txtAuthorizedPerson.Text = "";
            txtEndDate.Text = "";
            txtVerificationKey.Text = "";
            txtLicenceKey.Text = "";
            rbWorkStation.IsChecked = true;
        }

        private void ExportFile_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (txtVerificationKey.Text == "" || txtLicenceKey.Text == "")
                btnExportFile.IsEnabled = false;
            else
                btnExportFile.IsEnabled = true;
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

        #endregion

        #region Lisans Anahtarı Çöz
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

        private void btnDecrypt_Click(object sender, RoutedEventArgs e)
        {
            txtDecyrptedLicenseKey.Text = DecryptString(key, txtLicenseKeyDec.Text);
        }
        #endregion

        #region Lisans Anahtarları

        private void txtSearchBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            CollectionViewSource.GetDefaultView(listViewLicenses.ItemsSource).Refresh();
        }

        private bool LicenseFilter(object item)
        {
            if (cbLicensesColumn.SelectedIndex == 0)
            {
                if (String.IsNullOrEmpty(txtSearchBox.Text))
                    return true;
                else
                    return ((item as License).UniqKey.IndexOf(txtSearchBox.Text, StringComparison.OrdinalIgnoreCase) >= 0);
            }
            else if (cbLicensesColumn.SelectedIndex == 1)
            {
                if (String.IsNullOrEmpty(txtSearchBox.Text))
                    return true;
                else
                    return ((item as License).DealerName.IndexOf(txtSearchBox.Text, StringComparison.OrdinalIgnoreCase) >= 0);
            }
            else if (cbLicensesColumn.SelectedIndex == 2)
            {
                if (String.IsNullOrEmpty(txtSearchBox.Text))
                    return true;
                else
                    return ((item as License).CustomerName.IndexOf(txtSearchBox.Text, StringComparison.OrdinalIgnoreCase) >= 0);
            }
            else if (cbLicensesColumn.SelectedIndex == 3)
            {
                if (String.IsNullOrEmpty(txtSearchBox.Text))
                    return true;
                else
                    return ((item as License).AuthorizedPerson.IndexOf(txtSearchBox.Text, StringComparison.OrdinalIgnoreCase) >= 0);
            }
            else if (cbLicensesColumn.SelectedIndex == 4)
            {
                if (String.IsNullOrEmpty(txtSearchBox.Text))
                    return true;
                else
                    return ((item as License).CreatedDate.ToString().IndexOf(txtSearchBox.Text, StringComparison.OrdinalIgnoreCase) >= 0);
            }
            else if (cbLicensesColumn.SelectedIndex == 5)
            {
                if (String.IsNullOrEmpty(txtSearchBox.Text))
                    return true;
                else
                    return ((item as License).SupportEndDate.ToString().IndexOf(txtSearchBox.Text, StringComparison.OrdinalIgnoreCase) >= 0);
            }
            else if (cbLicensesColumn.SelectedIndex == 6)
            {
                if (String.IsNullOrEmpty(txtSearchBox.Text))
                    return true;
                else
                    return ((item as License).LicenseVersion.ToString().IndexOf(txtSearchBox.Text, StringComparison.OrdinalIgnoreCase) >= 0);
            }
            else
            {
                if (String.IsNullOrEmpty(txtSearchBox.Text))
                    return true;
                else
                    return ((item as License).Key.IndexOf(txtSearchBox.Text, StringComparison.OrdinalIgnoreCase) >= 0);
            }

        }

        private void cbLicensesColumn_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                txtSearchBox.Focus();
            }
        }

        private void RefreshLicenses()
        {
            listViewLicenses.ItemsSource = _licenseDal.GetList();
            CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewLicenses.ItemsSource);
            view.Filter = LicenseFilter;
        }

        private void listViewLicenses_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (listViewLicenses.SelectedIndex != -1)
            {
                var licenses = (License)listViewLicenses.SelectedItem;
                KeyInfoWindow keyInfoWindow = new KeyInfoWindow(licenses);
                keyInfoWindow.ShowDialog();
            }
        }

        private void btnShowMore_Click(object sender, RoutedEventArgs e)
        {
            if (listViewLicenses.SelectedIndex != -1)
            {
                var licenses = (License)listViewLicenses.SelectedItem;
                KeyInfoWindow keyInfoWindow = new KeyInfoWindow(licenses);
                keyInfoWindow.ShowDialog();
            }
        }

        #endregion

        private GridViewColumnHeader listViewLicensesCol = null;
        private SortAdorner listViewLicensesAdorner = null;

        private void listviewLicensesColumnSort_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewLicensesCol != null)
            {
                AdornerLayer.GetAdornerLayer(listViewLicensesCol).Remove(listViewLicensesAdorner);
                listViewLicenses.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewLicensesCol == column && listViewLicensesAdorner.Direction == newDir)
                newDir = ListSortDirection.Descending;

            listViewLicensesCol = column;
            listViewLicensesAdorner = new SortAdorner(listViewLicensesCol, newDir);
            AdornerLayer.GetAdornerLayer(listViewLicensesCol).Add(listViewLicensesAdorner);
            listViewLicenses.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));

        }
    }
}
