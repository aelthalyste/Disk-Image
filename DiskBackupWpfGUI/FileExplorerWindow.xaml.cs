using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
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
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.Entities.Concrete;
using NarDIWrapper;

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for FileExplorer.xaml
    /// </summary>
    public partial class FileExplorerWindow : Window
    {
        IBackupService _backupManager;
        List<FilesInBackup> _filesInBackupList = new List<FilesInBackup>();
        private bool _fileAllControl;

        public FileExplorerWindow(IBackupService backupManager)
        {
            InitializeComponent();

            BackupInfo backupInfo = new BackupInfo();

            _backupManager.InitFileExplorer(backupInfo);
            _filesInBackupList = _backupManager.GetFileInfoList();

            RemoveSystemFiles();

            listViewFileExplorer.ItemsSource = _filesInBackupList;
            SortItems();
            txtfileExplorerPath.Text = _backupManager.GetCurrentDirectory();
            _backupManager = backupManager;
        }

        #region Title Bar
        private void btnClose_Click(object sender, RoutedEventArgs e)
        {
            _backupManager.FreeFileExplorer();
            Close();
        }

        private void btnMin_Click(object sender, RoutedEventArgs e)
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

        private void btnFolder_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new System.Windows.Forms.FolderBrowserDialog();
            System.Windows.Forms.DialogResult result = dialog.ShowDialog();
            txtFolderPath.Text = dialog.SelectedPath;
        }

        private void btnBack_Click(object sender, RoutedEventArgs e)
        {
            _backupManager.PopDirectory();
            _filesInBackupList = _backupManager.GetFileInfoList();
            RemoveSystemFiles();
            listViewFileExplorer.ItemsSource = _filesInBackupList;
            SortItems();
            txtfileExplorerPath.Text = _backupManager.GetCurrentDirectory();
            // pop diyip
            // getFilesInCurrentDirectory
        }

        private void listViewFileExplorer_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            FilesInBackup filesInBackup = (FilesInBackup)listViewFileExplorer.SelectedItem;

            foreach (var item in _filesInBackupList)
            {
                if(item.Name.Equals(filesInBackup.Name) && item.StrSize.Equals(filesInBackup.StrSize))
                {
                    _backupManager.GetSelectedFileInfo(item);
                    _filesInBackupList = _backupManager.GetFileInfoList();
                    RemoveSystemFiles();
                    listViewFileExplorer.ItemsSource = _filesInBackupList;
                    SortItems();
                    txtfileExplorerPath.Text = _backupManager.GetCurrentDirectory();
                    break;
                }
            }
        }

        private void SortItems()
        {
            CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(listViewFileExplorer.ItemsSource);
            view.SortDescriptions.Add(new SortDescription("Type", ListSortDirection.Descending));
            view.SortDescriptions.Add(new SortDescription("Name", ListSortDirection.Ascending));
        }

        private void RemoveSystemFiles()
        {
            for (int i = 0; i < _filesInBackupList.Count;)
            {
                if ((_filesInBackupList[i].Name.ToCharArray()[0].Equals('$') && !_filesInBackupList[i].Name.Equals("$Recycle.Bin")) || _filesInBackupList[i].Name.Equals("."))
                {
                    _filesInBackupList.Remove(_filesInBackupList[i]);
                    continue;
                }
                i++;
            }
        }

        #region Checkbox Operations

        private void chbFileExplorerSelectAll_Checked(object sender, RoutedEventArgs e)
        {
            _fileAllControl = true;

            foreach (FilesInBackup item in listViewFileExplorer.ItemsSource)
            {
                listViewFileExplorer.SelectedItems.Add(item);
            }
        }

        private void chbFileExplorerSelectAll_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_fileAllControl)
            {
                foreach (FilesInBackup item in listViewFileExplorer.ItemsSource)
                {
                    listViewFileExplorer.SelectedItems.Remove(item);
                }
                _fileAllControl = true;
            }
        }

        private void CheckBox_Checked(object sender, RoutedEventArgs e)
        {
            if (chbFileExplorerSelectAll.IsChecked == false)
            {
                _fileAllControl = listViewFileExplorer.SelectedItems.Count == listViewFileExplorer.Items.Count;
                chbFileExplorerSelectAll.IsChecked = _fileAllControl;
            }
        }

        private void CheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            _fileAllControl = false;
            chbFileExplorerSelectAll.IsChecked = false;
        }

        #endregion

        //Seçilen değere gitmek için ise CW_SelectDirectory(seçilenID)
        //tekrardan getFilesInCurrentDirectory istenecek
    }
}