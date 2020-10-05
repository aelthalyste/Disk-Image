using DiskBackupWpfGUI.Model;
using System;
using System.Collections.Generic;
using System.Diagnostics;
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
        private bool _diskAllControl = false;
        private bool _restoreDiskAllControl = false;

        public MainWindow()
        {
            InitializeComponent();

            DiskInfo diskInfo = new DiskInfo { Size = "55 GB", BootType = "MBR", Format = "NTFS", Name = "Disk 1", VolumeSize = "100 GB", VolumeName = "Local Volume (D:)" };
            var page = new DiskInfoPage(diskInfo);
            var frame = new Frame();
            frame.Content = page;
            frame.VerticalAlignment = VerticalAlignment.Top;
            diskInfoStackPanel.Children.Add(frame);
            page = new DiskInfoPage(diskInfo);
            frame = new Frame();
            frame.Content = page;
            frame.VerticalAlignment = VerticalAlignment.Top;
            stackTasksDiskInfo.Children.Add(frame);
            diskInfo.Name = "Disk 2";
            page = new DiskInfoPage(diskInfo);
            frame = new Frame();
            frame.Content = page;
            frame.VerticalAlignment = VerticalAlignment.Top;
            diskInfoStackPanel.Children.Add(frame);
            page = new DiskInfoPage(diskInfo);
            frame = new Frame();
            frame.Content = page;
            frame.VerticalAlignment = VerticalAlignment.Top;
            stackTasksDiskInfo.Children.Add(frame);


            List<Discs> discsItems = new List<Discs>();
            discsItems.Add(new Discs()
            {
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
                Volume = "Local Volume",
                Letter = 'D',
                Area = "75 GB",
                FreeSize = "20 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 2"
            });

            listViewDisk.ItemsSource = discsItems;
            listViewRestoreDisk.ItemsSource = discsItems;

            CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewDisk.ItemsSource);
            PropertyGroupDescription groupDescription = new PropertyGroupDescription("DiscName");
            view.GroupDescriptions.Add(groupDescription);
        }

        private void OnNavigate(object sender, RequestNavigateEventArgs e)
        {
            if (e.Uri.IsAbsoluteUri)
                Process.Start(e.Uri.AbsoluteUri);

            e.Handled = true;
        }

        public class Discs
        {
            public string Volume { get; set; }
            public char Letter { get; set; }
            public string Area { get; set; }
            public string FreeSize { get; set; }
            public string Type { get; set; }
            public string FileSystem { get; set; }
            public string Statu { get; set; }
            public string DiscName { get; set; }
        }

        #region Title Bar
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
        #endregion

        #region Tasks Create Tab

        private void btnCreateTask_Click(object sender, RoutedEventArgs e)
        {
            NewCreateTask newCreateTask = new NewCreateTask();
            newCreateTask.ShowDialog();
        }

        #region Checkbox Operations
        private void chbDiskSelectDiskAll_Checked(object sender, RoutedEventArgs e)
        {
            if (!_diskAllControl)
            {
                foreach (Discs item in listViewDisk.ItemsSource)
                {
                    listViewDisk.SelectedItems.Add(item);
                }
                _diskAllControl = true;
            }
        }

        private void chbDiskSelectDiskAll_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_diskAllControl)
            {
                listViewDisk.SelectedItems.Clear();
                _diskAllControl = false;
            }
        }

        private void chbDisk_Checked(object sender, RoutedEventArgs e)
        {
            if (chbDiskSelectDiskAll.IsChecked == false)
            {
                _diskAllControl = listViewDisk.SelectedItems.Count == listViewDisk.Items.Count;
                chbDiskSelectDiskAll.IsChecked = _diskAllControl;
            }
        }

        private void chbDisk_Unchecked(object sender, RoutedEventArgs e)
        {
            _diskAllControl = false;
            chbDiskSelectDiskAll.IsChecked = false;
        }
        #endregion



        #endregion

        #region Tasks Tab

        #endregion

        #region Restore Tab

        #region Checkbox Operations
        private void chbRestoreDiskSelectAll_Checked(object sender, RoutedEventArgs e)
        {
            if (!_restoreDiskAllControl)
            {
                foreach (Discs item in listViewRestoreDisk.ItemsSource)
                {
                    listViewRestoreDisk.SelectedItems.Add(item);
                }
                _restoreDiskAllControl = true;
            }
        }

        private void chbRestoreDiskSelectAll_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_restoreDiskAllControl)
            {
                listViewRestoreDisk.SelectedItems.Clear();
                _restoreDiskAllControl = false;
            }
        }

        private void chbRestoreDisk_Checked(object sender, RoutedEventArgs e)
        {
            if (chbRestoreDiskSelectAll.IsChecked == false)
            {
                _restoreDiskAllControl = listViewRestoreDisk.SelectedItems.Count == listViewRestoreDisk.Items.Count;
                chbRestoreDiskSelectAll.IsChecked = _restoreDiskAllControl;
            }
        }

        private void chbRestoreDisk_Unchecked(object sender, RoutedEventArgs e)
        {
            _restoreDiskAllControl = false;
            chbRestoreDiskSelectAll.IsChecked = false;
        }

        #endregion



        #endregion

        #region View Backups Tab

        #endregion

        #region Backup Area Tab
        private void btnBackupAreaAdd_Click(object sender, RoutedEventArgs e)
        {
            AddBackupArea addBackupArea = new AddBackupArea();
            addBackupArea.ShowDialog();
        }

        #endregion

        #region Log Tab

        #endregion

        #region Settings Tab

        #region Upload
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
        private void rbtnUpload_Checked(object sender, RoutedEventArgs e)
        {
            stackUpload.IsEnabled = true;
        }

        private void rbtnUpload_Unchecked(object sender, RoutedEventArgs e)
        {
            stackUpload.IsEnabled = false;
        }
        #endregion

        #region Download
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
        private void rbtnDownload_Checked(object sender, RoutedEventArgs e)
        {
            stackDownload.IsEnabled = true;
        }

        private void rbtnDownload_Unchecked(object sender, RoutedEventArgs e)
        {
            stackDownload.IsEnabled = false;
        }
        #endregion



        #endregion

        #region Help Tab

        #endregion


        private void btnTaskOpen_Click(object sender, RoutedEventArgs e)
        {
            Statuses taskStatus = new Statuses(0);
            taskStatus.Show();
        }

        private void btnFilesBrowse_Click(object sender, RoutedEventArgs e)
        {
            Statuses backupStatus = new Statuses(1);
            backupStatus.Show();
        }

        private void btnTaskCopy_Click(object sender, RoutedEventArgs e)
        {
            Statuses restoreStatus = new Statuses(2);
            restoreStatus.Show();
        }

        private void btnRestore_Click(object sender, RoutedEventArgs e)
        {
            Restore restore = new Restore();
            try
            {
                restore.ShowDialog();
            }
            catch
            {
                MessageBox.Show("Geçmişe dönük restore yapılamaz." + e.ToString());
            }
        }

        private void checkBootPartition_Checked(object sender, RoutedEventArgs e)
        {
            stackBootCheck.IsEnabled = true;
        }

        private void checkBootPartition_Unchecked(object sender, RoutedEventArgs e)
        {
            stackBootCheck.IsEnabled = false;
            rbBootGPT.IsChecked = true;
            rbBootGPT.IsChecked = false;
        }

        private static T FindParent<T>(DependencyObject dependencyObject) where T : DependencyObject
        {
            var parent = VisualTreeHelper.GetParent(dependencyObject);
            if (parent == null)
                return null;

            var parentT = parent as T;
            return parentT ?? FindParent<T>(parent);
        }

        private void HeaderCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            var checkBox = e.OriginalSource as CheckBox;
            var expander = FindParent<Expander>(checkBox);
            var headerCheckBox = expander.FindName("HeaderCheckBox") as CheckBox;

            foreach (Discs item in listViewDisk.Items)
            {
                if (item.DiscName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewDisk.SelectedItems.Add(item);
                }
            }
        }

        private void HeaderCheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            var checkBox = e.OriginalSource as CheckBox;
            var expander = FindParent<Expander>(checkBox);
            var headerCheckBox = expander.FindName("HeaderCheckBox") as CheckBox;
            foreach (Discs item in listViewDisk.Items)
            {
                if (item.DiscName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewDisk.SelectedItems.Remove(item);
                }
            }
        }
    }
}
