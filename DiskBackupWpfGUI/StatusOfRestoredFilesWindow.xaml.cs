using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using Serilog;
using System;
using System.Collections;
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

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for StatusOfRestoredFilesWindow.xaml
    /// </summary>
    public partial class StatusOfRestoredFilesWindow : Window
    {
        private readonly IConfigurationDataDal _configurationDataDal;
        private readonly IBackupService _backupService;
        private readonly ILogger _logger;

        public StatusOfRestoredFilesWindow(List<FilesInBackup> filesInBackups, string targetPath, IConfigurationDataDal configurationDataDal, IBackupService backupService, IBackupService backupSerive, ILogger logger)
        {
            InitializeComponent();
            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);
            _backupService = backupSerive;
            _logger = logger.ForContext<StatusOfRestoredFilesWindow>();

            StartRestoringFiles(filesInBackups, targetPath);
        }

        private async void StartRestoringFiles(List<FilesInBackup> filesInBackups, string targetPath)
        {
            await Task.Delay(500);
            var restoreFiles = new List<RestoreFileInfo>();
            listViewRestoreFiles.ItemsSource = restoreFiles;
            foreach (FilesInBackup item in filesInBackups)
            {
                try
                {
                    txtFileName.Text = item.Name;
                    var result = _backupService.RestoreFilesInBackup(item, targetPath);
                    restoreFiles.Add(new RestoreFileInfo()
                    {
                        FileName = item.Name,
                        Status = result,
                        StrStatus = Resources[result.ToString()].ToString()
                    });
                    listViewRestoreFiles.Items.Refresh();
                    MessageBox.Show($"{item.Name} {result}", Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
                catch (Exception ex)
                {
                    _logger.Error(ex, $"Dosya restore işlemi gerçekleştirilemedi. {"item.Id: " + item.Id + " txtFolderPath.Text: " + targetPath}");
                }
            }
        }

        #region Title Bar
        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }

        private void btnRestoreFileClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        #endregion

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
    }
}
