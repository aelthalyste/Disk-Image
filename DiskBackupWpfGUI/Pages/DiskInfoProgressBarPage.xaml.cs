using DisckBackup.Entities;
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
            volumeNameTextBlock.Text = volumeInfo.Name;
            volumeLetterTextBlock.Text = volumeInfo.Letter.ToString();
            formatTextBlock.Text = volumeInfo.Format;
            volumeSizeTextBlock.Text = volumeInfo.StrSize;
            prioritySectionTextBlock.Text = volumeInfo.PrioritySection;
            pbVolume.Maximum = volumeInfo.Size;
            pbVolume.Value = volumeInfo.Size - volumeInfo.FreeSize;
        }
    }
}
