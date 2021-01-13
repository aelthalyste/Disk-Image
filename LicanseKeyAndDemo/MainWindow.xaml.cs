using LicanseKeyAndDemo;
using Microsoft.Win32;
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
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace LicenseKeyAndDemo
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();

            var key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\NarDiskBackup");

            if (key == null)
            {
                LicenseControllerWindow licenseControllerWindow = new LicenseControllerWindow(false);
                licenseControllerWindow.ShowDialog();
                if (!licenseControllerWindow._validate)
                {
                    Close();
                }
            }
            else // dosya var
            {
                if (key.GetValue("Type").ToString() == "1505") // gün kontrolleri yapılacak
                {
                    try
                    {
                        if (Convert.ToDateTime(key.GetValue("UploadDate").ToString()) <= DateTime.Now && Convert.ToDateTime(key.GetValue("ExpireDate").ToString()) >= DateTime.Now)
                        {
                            // uygulama çalışabilir
                        }
                        else // deneme süresi doldu
                        {
                            LicenseControllerWindow licenseControllerWindow = new LicenseControllerWindow(true);
                            licenseControllerWindow.ShowDialog(); // kontrol yolla demo seçeneğini kaldır
                            if (!licenseControllerWindow._validate)
                            {
                                Close();
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show("Dosyalar bozulmuş\n" + ex);
                        FixBrokenRegistry(); // registry ile oynanmış
                    }
                }
                else if (key.GetValue("Type").ToString() == "2606") // lisanslı
                {
                    // uygulama çalışabilir
                    btnValidateLicense.Visibility = Visibility.Collapsed;
                }
                else
                {
                    FixBrokenRegistry(); // registry ile oynanmış
                }
            }

        }

        private void FixBrokenRegistry()
        {
            var key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\NarDiskBackup", true);

            key.SetValue("UploadDate", DateTime.Now);
            key.SetValue("ExpireDate", DateTime.Now - TimeSpan.FromDays(1));
            key.SetValue("DaysLeft", 0);
            key.SetValue("Type", 1505);

            LicenseControllerWindow licenseControllerWindow = new LicenseControllerWindow(true);
            licenseControllerWindow.ShowDialog(); // kontrol yolla demo seçeneğini kaldır
            if (!licenseControllerWindow._validate)
            {
                Close();
            }
        }

        private void btnValidateLicense_Click(object sender, RoutedEventArgs e)
        {
            LicenseControllerWindow licenseControllerWindow = new LicenseControllerWindow(true);
            licenseControllerWindow.ShowDialog();
            if (!licenseControllerWindow._validate)
            {
                Close();
            }
        }
    }






    /*            
     *      var key = (Registry.CurrentUser.OpenSubKey("Console"));
            foreach (var item in key.GetSubKeyNames())
            {
                Console.WriteLine("mrb: " + item);
            }
            if (key.OpenSubKey("deneme") == null)
                Console.WriteLine("null geldi");
            else
                Console.WriteLine("hiçbiri gelmedi");
            Console.WriteLine(key.OpenSubKey("Git CMD"));
            var key2 = key.OpenSubKey("Git CMD");
            foreach (var item in key2.GetValueNames())
            {
                Console.WriteLine("slm şeker: " + item);
            }
            Console.WriteLine("---" + key);*/
}
