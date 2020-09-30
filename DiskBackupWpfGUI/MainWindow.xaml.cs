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
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private bool diskAllControl = false;
        public MainWindow()
        {
            InitializeComponent();

            List<Discs> discsItems = new List<Discs>();
            discsItems.Add(new Discs() { 
                checkSelect = false, 
                Volume = "System Reserverd", 
                Letter = 'A', 
                Area = "2 GB", 
                FreeSize = "1 GB", 
                Type = "GPT", 
                FileSystem = "NTFS", 
                Statu = "Sağlıklı",
                DiscName = "Disk 1"
            });
            discsItems.Add(new Discs()
            {
                checkSelect = false,
                Volume = "Local Volume",
                Letter = 'B',
                Area = "75 GB",
                FreeSize = "20 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 1"
            });
            discsItems.Add(new Discs()
            {
                checkSelect = false,
                Volume = "System Reserverd",
                Letter = 'C',
                Area = "2 GB",
                FreeSize = "1 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 2"
            });
            discsItems.Add(new Discs()
            {
                checkSelect = true,
                Volume = "Local Volume",
                Letter = 'D',
                Area = "75 GB",
                FreeSize = "20 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 2"
            });

            diskListBox.ItemsSource = discsItems;

            CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(diskListBox.ItemsSource);
            PropertyGroupDescription groupDescription = new PropertyGroupDescription("DiscName");
            view.GroupDescriptions.Add(groupDescription);
        }

        public class Discs
        {
            public bool checkSelect { get; set; }
            public string Volume { get; set; }
            public char Letter { get; set; }
            public string Area { get; set; }
            public string FreeSize { get; set; }
            public string Type { get; set; }
            public string FileSystem { get; set; }
            public string Statu { get; set; }
            public string DiscName { get; set; }
        }

        private void chbDiskSelectDiskAll_Checked(object sender, RoutedEventArgs e)
        {
            foreach (Discs item in diskListBox.ItemsSource)
            {
                item.checkSelect = true;
                diskListBox.SelectedItems.Add(item);
            }
        }

        private void chbDiskSelectDiskAll_Unchecked(object sender, RoutedEventArgs e)
        {
            if (diskAllControl == false)
            {
                foreach (Discs item in diskListBox.ItemsSource)
                {
                    item.checkSelect = false;
                    diskListBox.SelectedItems.Remove(item);
                }
            }
        }

        private void chbDisk_Checked(object sender, RoutedEventArgs e)
        {
            if (chbDiskSelectDiskAll.IsChecked == false)
            {
                bool diskControl = true;
                foreach (Discs item in diskListBox.ItemsSource)
                {
                    MessageBox.Show(item.Letter.ToString() + " " + item.checkSelect.ToString());
                    if (item.checkSelect == false)
                    {
                        diskControl = false;
                        break;
                    }
                }
                if (diskControl == true)
                {
                    chbDiskSelectDiskAll.IsChecked = true;
                }
            }
        }

        private void chbDisk_Unchecked(object sender, RoutedEventArgs e)
        {
            chbDiskSelectDiskAll.IsChecked = false;
            diskAllControl = true;
        }

        /*
         *
         */

        private void btnCreateTask_Click(object sender, RoutedEventArgs e)
        {
            NewCreateTask newCreateTask = new NewCreateTask();
            newCreateTask.ShowDialog();
        }

        private void btnTaskOpen_Click(object sender, RoutedEventArgs e)
        {
            TaskStatus taskStatus = new TaskStatus();
            taskStatus.ShowDialog();
        }

        private void btnMainClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void btnMainMin_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }

        #region UpDownClicks
        private void btnUploadDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtUpload.Text);
            if (count != 0)
            {
                count -= 1;
                txtUpload.Text = count.ToString();
            }
        }

        private void btnUploadUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtUpload.Text);
            if (count != 999)
            {
                count += 1;
                txtUpload.Text = count.ToString();
            }
        }

        private void btnDownloadDown_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtDownload.Text);
            if (count != 0)
            {
                count -= 1;
                txtDownload.Text = count.ToString();
            }
        }

        private void btnDownloadUp_Click(object sender, RoutedEventArgs e)
        {
            var count = Convert.ToInt32(txtDownload.Text);
            if (count != 999)
            {
                count += 1;
                txtDownload.Text = count.ToString();
            }
        }
        #endregion

        private void rbtnDownload_Checked(object sender, RoutedEventArgs e)
        {
            stackDownload.IsEnabled = true;
        }

        private void rbtnDownload_Unchecked(object sender, RoutedEventArgs e)
        {
            stackDownload.IsEnabled = false;
        }

        private void rbtnUpload_Checked(object sender, RoutedEventArgs e)
        {
            stackUpload.IsEnabled = true;
        }

        private void rbtnUpload_Unchecked(object sender, RoutedEventArgs e)
        {
            stackUpload.IsEnabled = false;
        }

    }
}
