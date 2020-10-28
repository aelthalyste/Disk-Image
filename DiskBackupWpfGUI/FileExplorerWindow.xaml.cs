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
using DiskBackup.Entities.Concrete;
using NarDIWrapper;

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for FileExplorer.xaml
    /// </summary>
    public partial class FileExplorerWindow : Window
    {
        public FileExplorerWindow()
        {
            InitializeComponent();
            CSNarFileExplorer cSNarFileExplorer = new CSNarFileExplorer();
            cSNarFileExplorer.CW_Init('C', 0, "");

            var resultList = cSNarFileExplorer.CW_GetFilesInCurrentDirectory();
            List<FilesInBackup> filesInBackupList = new List<FilesInBackup>();
            foreach (var item in resultList)
            {
                filesInBackupList.Add(new FilesInBackup
                {
                    Name = item.Name,
                    Type = (FileType)item.IsDirectory, //Directory ise 1 
                    Size = (long)item.Size,
                    Id = (long)item.ID,
                    //UpdatedDate = item.CreationTime
                    //Path değeri Batudan isteyelim
                    //UpdatedDate dönüşü daha yok
                });
            }

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
            // pop diyip
            // getFilesInCurrentDirectory
        }

        //Seçilen değere gitmek için ise CW_SelectDirectory(seçilenID)
        //tekrardan getFilesInCurrentDirectory istenecek
    }
}
