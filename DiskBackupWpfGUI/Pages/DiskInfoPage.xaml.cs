using DiskBackupWpfGUI.Model;
using DiskBackupWpfGUI.Pages;
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

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for DiskInfoPage.xaml
    /// </summary>
    public partial class DiskInfoPage : Page
    {
        public DiskInfoPage(DiskInfo diskInfo)
        {
            InitializeComponent();
            diskNameTextBlock.Text = diskInfo.Name;
            bootTypeTextBlock.Text = diskInfo.BootType;
            sizeTextBlock.Text = diskInfo.Size;

            foreach (var item in diskInfo.volumeInfos)
            {
                var page = new DiskInfoProgressBarPage(item);
                var frame = new Frame();
                frame.Content = page;
                diskInfoUniformGrid.Children.Add(frame);
            }

        }
    }
}
