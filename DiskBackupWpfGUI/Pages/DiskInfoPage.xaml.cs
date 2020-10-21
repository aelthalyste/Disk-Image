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
using DiskBackup.Entities.Concrete;

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for DiskInfoPage.xaml
    /// </summary>
    public partial class DiskInfoPage : Page
    {
        public DiskInfoPage(DiskInformation diskInfo)
        {
            InitializeComponent();
            diskNameTextBlock.Text = diskInfo.VolumeInfos[0].DiskName;
            bootTypeTextBlock.Text = diskInfo.VolumeInfos[0].DiskType;
            sizeTextBlock.Text = diskInfo.StrSize;

            foreach (var item in diskInfo.VolumeInfos)
            {
                var page = new DiskInfoProgressBarPage(item);
                var frame = new Frame();
                frame.Content = page;
                diskInfoUniformGrid.Children.Add(frame);
            }

        }
    }
}
