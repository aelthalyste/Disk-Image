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
        // 0 demo - 1 lisanslı
        public LicenseControllerWindow()
        {
            InitializeComponent();
        }

        private void save_Click(object sender, RoutedEventArgs e)
        {
            var key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\NarDiskBackup");
            if (key == null)
            {
                key = Registry.CurrentUser.CreateSubKey("SOFTWARE\\NarDiskBackup");
            }

            if (rbDemo.IsChecked == true)
            {
                key.SetValue("UploadDate", DateTime.Now);
                key.SetValue("ExpireDate", DateTime.Now + TimeSpan.FromDays(30));
                key.SetValue("DaysLeft", 30);
                key.SetValue("Type", 0);
            }
            else // lisans seçili
            {
                // KEYE GEÇİNCE DÜŞÜNÜLECEK
                // key kontrollerinden sonra bu gerçekleştirilecek
                key.SetValue("UploadDate", DateTime.Now);
                key.SetValue("ExpireDate", "");
                key.SetValue("DaysLeft", "");
                key.SetValue("Type", 1);
            }
            Close();
        }
    }
}
