using DiskBackupWpfGUI.Model;
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
            volumeNameTextBlock.Text = diskInfo.VolumeName;
            formatTextBlock.Text = diskInfo.Format;
            volumeSizeTextBlock.Text = diskInfo.VolumeSize;
        }
    }
}
