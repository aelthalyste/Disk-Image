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

namespace DiskBackupWpfGUI.Pages
{
    /// <summary>
    /// Interaction logic for DiskInfoProgressBarPage.xaml
    /// </summary>
    public partial class DiskInfoProgressBarPage : Page
    {
        public DiskInfoProgressBarPage(VolumeInfo volumeInfo)
        {
            InitializeComponent();
            volumeNameTextBlock.Text = volumeInfo.VolumeName;
            formatTextBlock.Text = volumeInfo.Format;
            volumeSizeTextBlock.Text = volumeInfo.VolumeSize;
            prioritySectionTextBlock.Text = volumeInfo.PrioritySection;
        }
    }
}
