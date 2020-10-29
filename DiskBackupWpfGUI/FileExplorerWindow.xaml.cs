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
        BackupManager _backupManager = new BackupManager();

        public FileExplorerWindow()
        {
            InitializeComponent();

            BackupInfo backupInfo = new BackupInfo();
            _backupManager.InitFileExplorer(backupInfo);
            //CSNarFileExplorer cSNarFileExplorer = new CSNarFileExplorer();
            //cSNarFileExplorer.CW_Init('C', 0, "");

            List<FilesInBackup> filesInBackupList = _backupManager.GetFileInfoList();
            //var resultList = cSNarFileExplorer.CW_GetFilesInCurrentDirectory();
            //List<FilesInBackup> filesInBackupList = new List<FilesInBackup>();
            //foreach (var item in resultList)
            //{
            //    filesInBackupList.Add(new FilesInBackup
            //    {
            //        Name = item.Name,
            //        Type = (FileType)item.IsDirectory, //Directory ise 1 
            //        Size = (long)item.Size,
            //        Id = (long)item.ID,
            //        //UpdatedDate = item.CreationTime
            //        //Path değeri Batudan isteyelim
            //        //UpdatedDate dönüşü daha yok
            //    });
            //}

            filesInBackupList.Add(new FilesInBackup
            {
                Name = "Eyüp",
                Type = (FileType)1, //Directory ise 1 
                Size = 29,
                Id = 1,
                //UpdatedDate = item.CreationTime
                //Path değeri Batudan isteyelim
                //UpdatedDate dönüşü daha yok
            });

            listViewFileExplorer.ItemsSource = filesInBackupList;
        }

        #region Title Bar
        private void btnClose_Click(object sender, RoutedEventArgs e)
        {
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
            List<FilesInBackup> filesInBackupList = _backupManager.GetFileInfoList();
            listViewFileExplorer.ItemsSource = filesInBackupList;
            // pop diyip
            // getFilesInCurrentDirectory
        }

        private void ListViewItem_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            MessageBox.Show(listViewFileExplorer.SelectedIndex.ToString());
            FilesInBackup filesInBackup = new FilesInBackup();
            filesInBackup.Id = listViewFileExplorer.SelectedIndex;
            _backupManager.GetSelectedFileInfo(filesInBackup);
            List<FilesInBackup> filesInBackupList = _backupManager.GetFileInfoList();
            listViewFileExplorer.ItemsSource = filesInBackupList;
        }

        //Seçilen değere gitmek için ise CW_SelectDirectory(seçilenID)
        //tekrardan getFilesInCurrentDirectory istenecek
    }
}