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
using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using DiskBackupWpfGUI.Utils;
using NarDIWrapper;
using Serilog;

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for FileExplorer.xaml
    /// </summary>
    public partial class FileExplorerWindow : Window
    {
        private IBackupService _backupManager;
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly ILogger _logger;
        private readonly IConfigurationDataDal _configurationDataDal;
        private readonly ILifetimeScope _scope;

        private List<FilesInBackup> _filesInBackupList = new List<FilesInBackup>();

        private bool _fileAllControl;

        public FileExplorerWindow(IBackupService backupManager, BackupInfo backupInfo, IBackupStorageDal backupStorageDal, ILogger logger, IConfigurationDataDal configurationDataDal, ILifetimeScope scope = null)
        {
            InitializeComponent();
            _backupStorageDal = backupStorageDal;
            _backupManager = backupManager;
            _configurationDataDal = configurationDataDal;
            _logger = logger.ForContext<FileExplorerWindow>();

            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            try
            {
                _backupManager.InitFileExplorer(backupInfo);
                _filesInBackupList = _backupManager.GetFileInfoList();
                RemoveSystemFiles();
                listViewFileExplorer.ItemsSource = _filesInBackupList;
                SortItems();
                txtfileExplorerPath.Text = _backupManager.GetCurrentDirectory();
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Beklenmedik hatadan dolayı file explorer açılamıyor.");
                MessageBox.Show(Resources["unexpectedErrorMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }
            _scope = scope;
        }

        #region Title Bar
        private void btnClose_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                _backupManager.FreeFileExplorer();
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Beklenmedik hata oluştu. |_backupManager.FreeFileExplorer()|");
            }
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
            using (var dialog = new System.Windows.Forms.FolderBrowserDialog())
            {
                var result = dialog.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    txtFolderPath.Text = dialog.SelectedPath;
                }
            }
        }

        private void btnBack_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                _backupManager.PopDirectory();
                _filesInBackupList = _backupManager.GetFileInfoList();
                RemoveSystemFiles();
                listViewFileExplorer.ItemsSource = _filesInBackupList;
                SortItems();
                txtfileExplorerPath.Text = _backupManager.GetCurrentDirectory();
            }
            catch(Exception ex)
            {
                _logger.Error(ex, "Beklenmedik hata oluştuğu için file explorerda geri gidilemiyor");
            }
        }

        private void listViewFileExplorer_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (listViewFileExplorer.SelectedIndex != -1 && ((FilesInBackup)listViewFileExplorer.SelectedItem).Type == FileType.Folder)
            {
                FilesInBackup filesInBackup = (FilesInBackup)listViewFileExplorer.SelectedItem;

                foreach (var item in _filesInBackupList)
                {
                    if (item.Name.Equals(filesInBackup.Name) && item.StrSize.Equals(filesInBackup.StrSize))
                    {
                        try
                        {
                            _backupManager.GetSelectedFileInfo(item);
                            _filesInBackupList = _backupManager.GetFileInfoList();
                            RemoveSystemFiles();
                            listViewFileExplorer.ItemsSource = _filesInBackupList;
                            SortItems();
                            txtfileExplorerPath.Text = _backupManager.GetCurrentDirectory();
                        }
                        catch(Exception ex)
                        {
                            _logger.Error(ex, "Beklenmedik hata oluştuğu için file explorerda içine girilemiyor");
                        }
                        break;
                    }
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

        private void btnRestore_Click(object sender, RoutedEventArgs e)
        {
            if (listViewFileExplorer.SelectedItems.Count != 0)
            {
                using (var scope = _scope.BeginLifetimeScope())
                {
                    var filesInBackups = new List<FilesInBackup>();
                    foreach (FilesInBackup item in listViewFileExplorer.SelectedItems)
                    {
                        filesInBackups.Add(item);
                    }
                    StatusOfRestoredFilesWindow statusOfRestoredFilesWindow = scope.Resolve<StatusOfRestoredFilesWindow>(new TypedParameter(filesInBackups.GetType(), filesInBackups), new TypedParameter(txtFolderPath.Text.GetType(), txtFolderPath.Text + @"\"));
                    statusOfRestoredFilesWindow.ShowDialog();
                }
                //Task.Run(() =>
                //{
                //    MessageBox.Show(Resources["filesRestoreStartedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                //});

                //foreach (FilesInBackup item in listViewFileExplorer.SelectedItems)
                //{
                //    try
                //    {
                //        var result = _backupManager.RestoreFilesInBackup(item, txtFolderPath.Text + @"\");
                //        MessageBox.Show($"{item.Name} {result}", Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                //    }
                //    catch (Exception ex)
                //    {
                //        _logger.Error(ex, $"Dosya restore işlemi gerçekleştirilemedi. {"item.Id: " + item.Id + " txtFolderPath.Text: " + txtFolderPath.Text}");
                //        MessageBox.Show(Resources["unexpectedErrorMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                //    }
                //}
            }
        }

        private void listViewFileExplorer_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (listViewFileExplorer.SelectedItems.Count != 0 && txtFolderPath.Text != null && txtFolderPath.Text != "")
                btnRestore.IsEnabled = true;
            else
                btnRestore.IsEnabled = false;
            foreach (FilesInBackup item in listViewFileExplorer.SelectedItems) // değiştir sunumdan sonra
            {
                if (item.Type == FileType.Folder)
                {
                    btnRestore.IsEnabled = false;
                    break;
                }
            }
        }

        private void txtFolderPath_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (listViewFileExplorer.SelectedItems.Count != 0 && txtFolderPath.Text != null && txtFolderPath.Text != "")
                btnRestore.IsEnabled = true;
            else
                btnRestore.IsEnabled = false;
            foreach (FilesInBackup item in listViewFileExplorer.SelectedItems) // değiştir sunumdan sonra
            {
                if (item.Type == FileType.Folder)
                {
                    btnRestore.IsEnabled = false;
                    break;
                }
            }
        }

        public void SetApplicationLanguage(string option)
        {
            ResourceDictionary dict = new ResourceDictionary();

            switch (option)
            {
                case "tr":
                    dict.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);
                    break;
                case "en":
                    dict.Source = new Uri("..\\Resources\\Lang\\string_eng.xaml", UriKind.Relative);
                    break;
                default:
                    dict.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);
                    break;
            }
            Resources.MergedDictionaries.Add(dict);
        }


        //Seçilen değere gitmek için ise CW_SelectDirectory(seçilenID)
        //tekrardan getFilesInCurrentDirectory istenecek
    }
}