using DisckBackup.Entities;
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
        private bool _diskAllHeaderControl = false;
        private bool _restoreDiskAllControl = false;
        private bool _restoreDiskAllHeaderControl = false;
        private List<CheckBox> _expanderCheckBoxes = new List<CheckBox>();
        private List<int> _numberOfItems = new List<int>();
        private List<string> _groupName = new List<string>();
        private List<CheckBox> _restoreExpanderCheckBoxes = new List<CheckBox>();
        private List<int> _restoreNumberOfItems = new List<int>();
        private List<string> _restoreGroupName = new List<string>();

        public MainWindow()
        {
            InitializeComponent();
            DiskInfo diskInfo = new DiskInfo { 
                StrSize = "400 GB", 
                Size = 1222,
                BootType = "MBR",         
                Name = "Disk 1"
             };
            diskInfo.volumeInfos.Add(new VolumeInfo
            {
                Format = "NTFS",
                StrSize = "100 GB",
                Name = "System Reserverd",
                PrioritySection = "Primary"
            });
            diskInfo.volumeInfos.Add(new VolumeInfo
            {
                Format = "NTFS",
                StrSize = "300 GB",
                Name = "Local Volume (C:)",
                PrioritySection = "Primary"
            });
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

            DiskInfo diskInfo2 = new DiskInfo
            {
                StrSize = "700 GB",
                BootType = "MBR",
                Name = "Disk 2"
            };
            diskInfo2.volumeInfos.Add(new VolumeInfo
            {
                Format = "NTFS",
                StrSize = "500 GB",
                Name = "System Reserverd",
                PrioritySection = "Primary"
            });
            diskInfo2.volumeInfos.Add(new VolumeInfo
            {
                Format = "NTFS",
                StrSize = "200 GB",
                Name = "Local Volume (C:)",
                PrioritySection = "Primary"
            });
            page = new DiskInfoPage(diskInfo2);
            frame = new Frame();
            frame.Content = page;
            frame.VerticalAlignment = VerticalAlignment.Top;
            diskInfoStackPanel.Children.Add(frame);
            page = new DiskInfoPage(diskInfo2);
            frame = new Frame();
            frame.Content = page;
            frame.VerticalAlignment = VerticalAlignment.Top;
            stackTasksDiskInfo.Children.Add(frame);


            List<Discs> discsItems = new List<Discs>();
            discsItems.Add(new Discs()
            {
                Volume = "System Reserverd",
                Letter = 'C',
                Area = "2 GB",
                FreeSize = "1 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 1",
                RealSize = 2
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
                DiscName = "Disk 3",
                RealSize = 75
            });
            discsItems.Add(new Discs()
            {
                Volume = "Local Volume",
                Letter = 'E',
                Area = "100 GB",
                FreeSize = "55 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 3",
                RealSize = 100
            });
            discsItems.Add(new Discs()
            {
                Volume = "Local Volume",
                Letter = 'F',
                Area = "500 GB",
                FreeSize = "120 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 1",
                RealSize = 500
            });
            discsItems.Add(new Discs()
            {
                Volume = "System Reserverd",
                Letter = 'C',
                Area = "128 GB",
                FreeSize = "1 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 2",
                RealSize = 128
            });
            discsItems.Add(new Discs()
            {
                Volume = "Local Volume",
                Letter = 'D',
                Area = "250 GB",
                FreeSize = "100 GB",
                Type = "GPT",
                FileSystem = "NTFS",
                Statu = "Sağlıklı",
                DiscName = "Disk 2",
                RealSize = 250
            });


            listViewDisk.ItemsSource = discsItems;
            listViewRestoreDisk.ItemsSource = discsItems;

            CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewDisk.ItemsSource);
            PropertyGroupDescription groupDescription = new PropertyGroupDescription("DiscName");
            view.GroupDescriptions.Add(groupDescription);

            List<Backups> backupsItems = new List<Backups>();
            backupsItems.Add(new Backups()
            {
                Type = "full",
                FileName = "Volume_C_Full"
            });
            backupsItems.Add(new Backups()
            {
                Type = "inc",
                FileName = "Disk 1"
            });
            backupsItems.Add(new Backups()
            {
                Type = "diff",
                FileName = "Disk 2",
                IsCloud = true
            });

            listViewBackups.ItemsSource = backupsItems;

            List<Log> logsItems = new List<Log>();
            logsItems.Add(new Log()
            {
                LogIcon = "success",
                StartDate = "06.10.2020 22:11"
            });
            logsItems.Add(new Log()
            {
                LogIcon = "fail",
                StartDate = "10.10.2020 10:20"
            });

            listViewLog.ItemsSource = logsItems;
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
            public long RealSize { get; set; }
        }

        public class Backups
        {
            public string Type { get; set; }
            public string FileName { get; set; }
            public bool IsCloud { get; set; } = false;
        }

        public class Log
        {
            public string LogIcon { get; set; }
            public string StartDate { get; set; }
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

        private void Expander_Loaded(object sender, RoutedEventArgs e)
        {
            var expander = sender as Expander;
            long diskSize = 0;

            foreach (Discs item in listViewDisk.Items)
            {
                if (item.DiscName.Equals(expander.Tag.ToString()))
                {
                    diskSize += item.RealSize;
                }
            }

            var size = expander.FindName("txtTotalSize") as TextBlock;
            var expanderCheck = expander.FindName("HeaderCheckBox") as CheckBox;
            _expanderCheckBoxes.Add(expanderCheck);
            var numberOfItems = expander.FindName("txtNumberOfItems") as TextBlock;
            _numberOfItems.Add(Convert.ToInt32(numberOfItems.Text));
            var groupName = expander.FindName("txtGroupName") as TextBlock;
            _groupName.Add(groupName.Text);
            size.Text = diskSize.ToString() + " GB";
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
                _expanderCheckBoxes.ForEach(cb => cb.IsChecked = true);
            }
        }

        private void chbDiskSelectDiskAll_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_diskAllControl)
            {
                _expanderCheckBoxes.ForEach(cb => cb.IsChecked = false);
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

            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as Discs; //data seçilen değer

            for (int i = 0; i < _groupName.Count; i++)
            {
                if (data.DiscName.Equals(_groupName[i]))
                {
                    int totalSelected = 0;
                    //i kaçıncı sıradaki adete eşit olacağı
                    foreach (Discs item in listViewDisk.SelectedItems)
                    {
                        if (item.DiscName.Equals(data.DiscName))
                        {
                            totalSelected++;
                        }
                    }
                    if (_numberOfItems[i] == totalSelected)
                    {
                        _expanderCheckBoxes[i].IsChecked = true;
                    }
                }
            }
        }

        private void chbDisk_Unchecked(object sender, RoutedEventArgs e)
        {
            _diskAllControl = false;
            chbDiskSelectDiskAll.IsChecked = false;
            _diskAllHeaderControl = false;

            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as Discs; //data seçilen değer

            for (int i = 0; i < _groupName.Count; i++)
            {
                if (_groupName[i].Equals(data.DiscName))
                {
                    _expanderCheckBoxes[i].IsChecked = false;
                    _diskAllHeaderControl = true;
                }
            }
        }

        private void HeaderCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            var headerCheckBox = sender as CheckBox;
            _diskAllHeaderControl = true;

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
            if (_diskAllHeaderControl)
            {
                var headerCheckBox = sender as CheckBox;
                foreach (Discs item in listViewDisk.Items)
                {
                    if (item.DiscName.Equals(headerCheckBox.Tag.ToString()))
                    {
                        listViewDisk.SelectedItems.Remove(item);
                    }
                }
                _diskAllHeaderControl = true;
            }
        }

        #endregion





        #endregion

        #region Tasks Tab

        #endregion

        #region Restore Tab

        private void Expander_Loaded_1(object sender, RoutedEventArgs e)
        {
            var expander = sender as Expander;
            long diskSize = 0;

            foreach (Discs item in listViewRestoreDisk.Items)
            {
                if (item.DiscName.Equals(expander.Tag.ToString()))
                {
                    diskSize += item.RealSize;
                }
            }

            var size = expander.FindName("txtRestoreTotalSize") as TextBlock;
            var expanderCheck = expander.FindName("cbRestoreHeader") as CheckBox;
            _restoreExpanderCheckBoxes.Add(expanderCheck);
            var numberOfItems = expander.FindName("txtRestoreNumberOfItems") as TextBlock;
            _restoreNumberOfItems.Add(Convert.ToInt32(numberOfItems.Text));
            var groupName = expander.FindName("txtRestoreGroupName") as TextBlock;
            _restoreGroupName.Add(groupName.Text);
            size.Text = diskSize.ToString() + " GB";
        }


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
                _restoreExpanderCheckBoxes.ForEach(cb => cb.IsChecked = true);
            }
        }

        private void chbRestoreDiskSelectAll_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_restoreDiskAllControl)
            {
                _restoreExpanderCheckBoxes.ForEach(cb => cb.IsChecked = false);
                listViewRestoreDisk.SelectedItems.Clear();
                _restoreDiskAllHeaderControl = false;
            }
        }

        private void chbRestoreDisk_Checked(object sender, RoutedEventArgs e)
        {
            if (chbRestoreDiskSelectAll.IsChecked == false)
            {
                _restoreDiskAllControl = listViewRestoreDisk.SelectedItems.Count == listViewRestoreDisk.Items.Count;
                chbRestoreDiskSelectAll.IsChecked = _restoreDiskAllControl;
            }

            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as Discs; //data seçilen değer

            for (int i = 0; i < _restoreGroupName.Count; i++)
            {
                if (data.DiscName.Equals(_restoreGroupName[i]))
                {
                    int totalSelected = 0;
                    //i kaçıncı sıradaki adete eşit olacağı
                    foreach (Discs item in listViewRestoreDisk.SelectedItems)
                    {
                        if (item.DiscName.Equals(data.DiscName))
                        {
                            totalSelected++;
                        }
                    }
                    if (_restoreNumberOfItems[i] == totalSelected)
                    {
                        _restoreExpanderCheckBoxes[i].IsChecked = true;
                    }
                }
            }
        }

        private void chbRestoreDisk_Unchecked(object sender, RoutedEventArgs e)
        {
            _restoreDiskAllControl = false;
            chbRestoreDiskSelectAll.IsChecked = false;
            _restoreDiskAllHeaderControl = false;

            var dataItem = FindParent<ListViewItem>(sender as DependencyObject);
            var data = dataItem.DataContext as Discs; //data seçilen değer

            for (int i = 0; i < _restoreGroupName.Count; i++)
            {
                if (_restoreGroupName[i].Equals(data.DiscName))
                {
                    _restoreExpanderCheckBoxes[i].IsChecked = false;
                    _restoreDiskAllHeaderControl = true;
                }
            }
        }

        private void cbRestoreHeader_Checked(object sender, RoutedEventArgs e)
        {
            var headerCheckBox = sender as CheckBox;
            _restoreDiskAllHeaderControl = true;

            foreach (Discs item in listViewRestoreDisk.Items)
            {
                if (item.DiscName.Equals(headerCheckBox.Tag.ToString()))
                {
                    listViewRestoreDisk.SelectedItems.Add(item);
                }
            }
        }

        private void cbRestoreHeader_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_restoreDiskAllHeaderControl)
            {
                var headerCheckBox = sender as CheckBox;
                foreach (Discs item in listViewRestoreDisk.Items)
                {
                    if (item.DiscName.Equals(headerCheckBox.Tag.ToString()))
                    {
                        listViewRestoreDisk.SelectedItems.Remove(item);
                    }
                }
                _restoreDiskAllHeaderControl = true;
            }
        }

        #endregion

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
            //Statuses backupStatus = new Statuses(1);
            //backupStatus.Show();
            FileExplorer fileExplorer = new FileExplorer();
            fileExplorer.Show();
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

        private static T FindParent<T>(DependencyObject dependencyObject) where T : DependencyObject
        {
            var parent = VisualTreeHelper.GetParent(dependencyObject);
            if (parent == null)
                return null;

            var parentT = parent as T;
            return parentT ?? FindParent<T>(parent);
        }


    }
}
