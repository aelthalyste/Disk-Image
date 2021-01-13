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
using System.Windows.Shapes;

namespace LicanseKeyAndDemo
{
    /// <summary>
    /// Interaction logic for LicenseControllerWindow.xaml
    /// </summary>
    public partial class LicenseControllerWindow : Window
    {
        // 1505 demo - 2606 lisanslı
        // windowType false standart, true lisans
        private bool _windowType;

        public LicenseControllerWindow(bool windowType)
        {
            InitializeComponent();

            _windowType = windowType;

            if (windowType)
            {
                rbDemo.Visibility = Visibility.Collapsed;
                rbLicense.IsChecked = true;
            }
        }

        private void save_Click(object sender, RoutedEventArgs e)
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
                key.SetValue("ExpireDate", DateTime.Now + TimeSpan.FromDays(30));
                key.SetValue("DaysLeft", 30);
                key.SetValue("Type", 1505);
            }
            else // lisans seçili
            {
                // KEYE GEÇİNCE DÜŞÜNÜLECEK
                // key kontrollerinden sonra bu gerçekleştirilecek
                key.SetValue("UploadDate", DateTime.Now);
                key.SetValue("ExpireDate", "");
                key.SetValue("DaysLeft", "");
                key.SetValue("Type", 2606);
            }
            Close();
        }
    }
}
