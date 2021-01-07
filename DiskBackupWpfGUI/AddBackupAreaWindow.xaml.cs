using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler;
using DiskBackupWpfGUI.Utils;
using Serilog;
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

namespace DiskBackupWpfGUI
{
    /// <summary>
    /// Interaction logic for AddBackupArea.xaml
    /// </summary>
    public partial class AddBackupAreaWindow : Window
    {
        private IBackupStorageService _backupStorageService;
        private ITaskSchedulerManager _taskSchedulerManager;
        private readonly IBackupStorageDal _backupStorageDal;
        private readonly ITaskInfoDal _taskInfoDal;
        private readonly ILogger _logger;

        private IConfigHelper _configHelper;

        private bool _showSettings = false;
        private bool _updateControl = false;
        private int _updateId;

        public AddBackupAreaWindow(IBackupStorageService backupStorageService, IBackupStorageDal backupStorageDal, ILogger logger, ITaskInfoDal taskInfoDal, 
            ITaskSchedulerManager taskSchedulerManager, IConfigHelper configHelper)
        {
            InitializeComponent();
            _updateControl = false;
            _backupStorageService = backupStorageService;
            _backupStorageDal = backupStorageDal;
            _taskInfoDal = taskInfoDal;
            _taskSchedulerManager = taskSchedulerManager;
            _logger = logger.ForContext<AddBackupAreaWindow>();
            _configHelper = configHelper;
            SetApplicationLanguage(_configHelper.GetConfig("lang"));
        }

        public AddBackupAreaWindow(BackupStorageInfo backupStorageInfo, IBackupStorageService backupStorageService, IBackupStorageDal backupStorageDal, ILogger logger, 
            ITaskInfoDal taskInfoDal, ITaskSchedulerManager taskSchedulerManager, IConfigHelper configHelper)
        {
            InitializeComponent();

            _logger = logger.ForContext<AddBackupAreaWindow>();
            _updateId = backupStorageInfo.Id;
            _updateControl = true;
            _backupStorageService = backupStorageService;
            _taskSchedulerManager = taskSchedulerManager;
            _backupStorageDal = backupStorageDal;
            _taskInfoDal = taskInfoDal;

            _configHelper = configHelper;
            SetApplicationLanguage(_configHelper.GetConfig("lang"));

            txtBackupAreaName.Text = backupStorageInfo.StorageName;
            txtBackupAreaDescription.Text = backupStorageInfo.Description;
            if (backupStorageInfo.IsCloud) //hibrit
            {
                cbBackupToCloud.IsChecked = true;
                if (backupStorageInfo.Username != null) // nas
                {
                    rbNAS.IsChecked = true;
                    txtSettingsNASFolderPath.Text = backupStorageInfo.Path.Substring(0, backupStorageInfo.Path.Length - 1);
                    txtSettingsNASDomain.Text = backupStorageInfo.Domain;
                    txtSettingsNASUserName.Text = backupStorageInfo.Username;
                    //txtSettingsNASPassword.Password = backupStorageInfo.Password;
                }
                else // yerel disktir
                {
                    rbLocalDisc.IsChecked = true;
                    txtSettingsFolderPath.Text = backupStorageInfo.Path;
                }
            }
            else // sadece nas veya yerel disk
            {
                cbBackupToCloud.IsChecked = false;
                if (backupStorageInfo.Username != null) // nas
                {
                    rbNAS.IsChecked = true;
                    txtSettingsNASFolderPath.Text = backupStorageInfo.Path.Substring(0, backupStorageInfo.Path.Length - 1);
                    txtSettingsNASDomain.Text = backupStorageInfo.Domain;
                    txtSettingsNASUserName.Text = backupStorageInfo.Username;
                    //txtSettingsNASPassword.Password = backupStorageInfo.Password;
                }
                else // yerel disktir
                {
                    rbLocalDisc.IsChecked = true;
                    txtSettingsFolderPath.Text = backupStorageInfo.Path;
                }
            }
        }

        #region Title Bar
        private void btnABAMin_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void btnABAClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }
        #endregion

        #region Next-Back-Ok-Cancel Button
        private void btnBackupAreaBack_Click(object sender, RoutedEventArgs e)
        {
            if (ABATabControl.SelectedIndex != 0)
            {
                ABATabControl.SelectedIndex -= 1;
            }
        }

        private void btnBackupAreaNext_Click(object sender, RoutedEventArgs e)
        {
            if (ABATabControl.SelectedIndex != 3)
            {
                ABATabControl.SelectedIndex += 1;
            }

            if (ABATabControl.SelectedIndex == 3)
            {
                if (rbLocalDisc.IsChecked.Value) // yerel disk
                {
                    txtName.Text = txtBackupAreaName.Text;
                    txtTarget.Text = Resources["localDisc"].ToString();
                    txtFolderPath.Text = txtSettingsFolderPath.Text;
                    if (cbBackupToCloud.IsChecked.Value)
                        txtBackupCloud.Text = Resources["active"].ToString();
                    else
                        txtBackupCloud.Text = Resources["inactive"].ToString();
                }
                else if (rbNAS.IsChecked.Value) // nas
                {
                    txtName.Text = txtBackupAreaName.Text;
                    txtTarget.Text = "NAS cihazı";
                    txtFolderPath.Text = txtSettingsNASFolderPath.Text;
                    if (cbBackupToCloud.IsChecked.Value)
                        txtBackupCloud.Text = Resources["active"].ToString();
                    else
                        txtBackupCloud.Text = Resources["inactive"].ToString();
                }
            }
        }

        private void btnBackupAreaOk_Click(object sender, RoutedEventArgs e)
        {
            bool controlFlag = true;
            //kaydet
            if (rbLocalDisc.IsChecked.Value) // yerel disk
            {
                if (txtBackupAreaName.Text.Equals("") || txtSettingsFolderPath.Text.Equals(""))
                {
                    MessageBox.Show(Resources["notNullMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    controlFlag = false;
                }
                else
                {
                    if (!(txtSettingsFolderPath.Text[1].Equals(':')))
                    {
                        MessageBox.Show(Resources["validateDirectoryMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                        controlFlag = false;
                    }
                    else
                    {
                        bool exists = System.IO.Directory.Exists(txtSettingsFolderPath.Text);

                        if (!exists)
                        {
                            try
                            {
                                System.IO.Directory.CreateDirectory(txtSettingsFolderPath.Text);
                                _logger.Information("{dizin} dizini oluşturuldu.", txtSettingsFolderPath.Text);
                            }
                            catch (Exception ex)
                            {
                                _logger.Error(ex, "{dizin} dizini oluşturulamadı.", txtSettingsFolderPath.Text);
                                MessageBox.Show(Resources["sameNameInDirectoryMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                                controlFlag = false;
                            }
                        }
                    }
                }
            }
            else if (rbNAS.IsChecked.Value) // nas
            {
                if (txtBackupAreaName.Text.Equals("") || txtSettingsNASFolderPath.Text.Equals("") || txtSettingsNASUserName.Text.Equals("") || txtSettingsNASPassword.Password.Equals("") || txtSettingsNASDomain.Text.Equals(""))
                {
                    MessageBox.Show(Resources["notNullMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    controlFlag = false;
                }
            }

            if (controlFlag)
            {
                if (rbLocalDisc.IsChecked.Value) // yerel disk
                {
                    BackupStorageInfo backupStorageInfo = new BackupStorageInfo
                    {
                        StorageName = txtBackupAreaName.Text,
                        Description = txtBackupAreaDescription.Text,
                        //Path = txtSettingsFolderPath.Text + @"\",
                        IsCloud = cbBackupToCloud.IsChecked.Value,
                    };

                    if (!(txtSettingsFolderPath.Text.Last().Equals('\\'))) // equals kontrolüne bak
                        backupStorageInfo.Path = txtSettingsFolderPath.Text + @"\";
                    else
                        backupStorageInfo.Path = txtSettingsFolderPath.Text;

                    backupStorageInfo.Type = BackupStorageType.Windows;

                    if (cbBackupToCloud.IsChecked.Value)
                    {
                        // bulut işlemleri gelecek (kullanılan alan vs...)
                    }

                    if (_updateControl)
                    {
                        //update
                        backupStorageInfo.Id = _updateId;
                        if (!(txtSettingsFolderPath.Text.Last().Equals('\\'))) // equals kontrolüne bak
                            backupStorageInfo.Path = txtSettingsFolderPath.Text + @"\";
                        else
                            backupStorageInfo.Path = txtSettingsFolderPath.Text;
                        // bozulacak restorelar var mı kontrolü ve etkilenecek backuplar kontrolü
                        if (AffectedTaskControl())
                        {
                            return;
                        }
                        UpdateAndShowResult(backupStorageInfo);
                    }
                    else
                    {
                        //kaydet
                        AddAndShowResult(backupStorageInfo);
                    }

                }
                else // Nas
                {
                    BackupStorageInfo backupStorageInfo = new BackupStorageInfo
                    {
                        StorageName = txtBackupAreaName.Text,
                        Description = txtBackupAreaDescription.Text,
                        Path = txtSettingsNASFolderPath.Text + @"\",
                        IsCloud = cbBackupToCloud.IsChecked.Value,
                        Domain = txtSettingsNASDomain.Text,
                        Username = txtSettingsNASUserName.Text,
                        Password = txtSettingsNASPassword.Password
                    };

                    backupStorageInfo.Type = BackupStorageType.NAS;

                    if (cbBackupToCloud.IsChecked.Value)
                    {
                        // bulut işlemleri gelecek (kullanılan alan vs...)
                    }

                    if (_updateControl)
                    {
                        //update
                        backupStorageInfo.Id = _updateId;
                        backupStorageInfo.Path = txtSettingsNASFolderPath.Text + @"\";
                        // bozulacak restorelar var mı kontrolü ve etkilenecek backuplar kontrolü
                        if (AffectedTaskControl())
                        {
                            return;
                        }
                        UpdateAndShowResult(backupStorageInfo);
                    }
                    else
                    {
                        //kaydet
                        AddAndShowResult(backupStorageInfo);
                    }
                }
            }
        }

        private void btnBackupAreaCancel_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
        #endregion

        private bool AffectedTaskControl()
        {
            Console.WriteLine("AffectedTaskControl girdi");
            var taskList = _taskInfoDal.GetList();
            bool taskRunnigFlag = false, restoreTaskFlag = false;
            foreach (TaskInfo itemTask in taskList)
            {
                if (itemTask.BackupStorageInfoId == _updateId && (itemTask.Status == TaskStatusType.Error || itemTask.Status == TaskStatusType.Paused || itemTask.Status == TaskStatusType.Working))
                    taskRunnigFlag = true;
                if (itemTask.BackupStorageInfoId == _updateId && itemTask.Type == TaskType.Restore)
                    restoreTaskFlag = true;
            }
            if (taskRunnigFlag)
            {
                MessageBox.Show(Resources["runningTaskAffectedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                return true;
            }
            if (restoreTaskFlag)
            {
                var result = MessageBox.Show(Resources["restoreTaskAffectedMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.YesNo, MessageBoxImage.Question);
                if (result == MessageBoxResult.No)
                {
                    return true;
                }
                // ilgili restoreları boz
                foreach (TaskInfo itemTask in taskList) //etkilenen taskları etkisizleştir
                {
                    if (itemTask.BackupStorageInfoId == _updateId && itemTask.Type == TaskType.Restore)
                    {
                        itemTask.EnableDisable = TecnicalTaskStatusType.Broken;
                        itemTask.BackupStorageInfoId = 0;

                        if (itemTask.ScheduleId != null && itemTask.ScheduleId != "")
                        {
                            try
                            {
                                _taskSchedulerManager.DeleteJob(itemTask.ScheduleId);
                            }
                            catch (Exception ex)
                            {
                                _logger.Error(ex, "Jobı silme işlemi gerçekleştirilemiyor. |_taskSchedulerManager.DeleteJob()|");
                            }
                            itemTask.ScheduleId = "";
                        }

                        _taskInfoDal.Update(itemTask);
                    }
                }
            }
            return false;
        }

        private void rbLocalDisc_Checked(object sender, RoutedEventArgs e)
        {
            _showSettings = false;
        }

        private void rbNAS_Checked(object sender, RoutedEventArgs e)
        {
            _showSettings = true;
        }

        private void ABATabControl_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (ABATabControl.SelectedIndex == 0)
            {
                lblTabHeader.Text = Resources["name"].ToString();
                lblTabContent.Text = Resources["ABANameContent"].ToString();
                btnBackupAreaBack.IsEnabled = false;
            }
            else if (ABATabControl.SelectedIndex == 1)
            {
                lblTabHeader.Text = Resources["target"].ToString();
                lblTabContent.Text = Resources["ABATargetContent"].ToString();
                btnBackupAreaBack.IsEnabled = true;
            }
            else if (ABATabControl.SelectedIndex == 2)
            {
                lblTabHeader.Text = Resources["settings"].ToString();
                lblTabContent.Text = Resources["ABASettingsContent"].ToString();
                if (_showSettings == false)
                {
                    stackLocalDisc.Visibility = Visibility.Visible;
                    stackNAS.Visibility = Visibility.Hidden;
                }
                else
                {
                    stackNAS.Visibility = Visibility.Visible;
                    stackLocalDisc.Visibility = Visibility.Hidden;
                }
                btnBackupAreaNext.IsEnabled = true;
            }
            else if (ABATabControl.SelectedIndex == 3)
            {
                lblTabHeader.Text = Resources["summary"].ToString();
                lblTabContent.Text = Resources["ABASummaryContent"].ToString();
                btnBackupAreaNext.IsEnabled = false;
            }
        }

        private void btnSettingsNASFolder_Click(object sender, RoutedEventArgs e)
        {
            using (var dialog = new System.Windows.Forms.OpenFileDialog())
            {
                dialog.Filter = "Folders|\n";
                dialog.ValidateNames = false;
                dialog.CheckFileExists = false;
                dialog.CheckPathExists = true;
                dialog.Multiselect = false;
                // Always default to Folder Selection.
                dialog.FileName = "Folder Selection.";

                var result = dialog.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    string folderSelectionDelete = dialog.FileName.Split('\\').Last();
                    txtSettingsNASFolderPath.Text = dialog.FileName.Substring(0, (dialog.FileName.Length - folderSelectionDelete.Length) - 1);
                }
            }
        }

        private void btnSettingsLocalFolder_Click(object sender, RoutedEventArgs e)
        {
            using (var dialog = new System.Windows.Forms.FolderBrowserDialog())
            {
                var result = dialog.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    txtSettingsFolderPath.Text = dialog.SelectedPath;
                }
            }
        }

        private void UpdateAndShowResult(BackupStorageInfo backupStorageInfo)
        {
            try
            {
                if (backupStorageInfo.Type == BackupStorageType.NAS)
                {
                    if (_backupStorageService.ValidateNasConnection(backupStorageInfo.Path.Substring(0, backupStorageInfo.Path.Length - 1), backupStorageInfo.Username, backupStorageInfo.Password, backupStorageInfo.Domain))
                    {
                        // güncelleme yap
                        var result = _backupStorageService.UpdateBackupStorage(backupStorageInfo);
                        if (result)
                        {
                            Close();
                            MessageBox.Show(Resources["updateSuccessMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                        }
                        else
                            MessageBox.Show(Resources["updateFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                    else
                    {
                        MessageBox.Show(Resources["incorrectNASMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
                else
                {
                    var result = _backupStorageService.UpdateBackupStorage(backupStorageInfo);
                    if (result)
                    {
                        Close();
                        MessageBox.Show(Resources["updateSuccessMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                    }
                    else
                        MessageBox.Show(Resources["updateFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Beklenmedik hatadan dolayı güncelleme işlemi başarısız oldu");
                MessageBox.Show(Resources["unexpectedUpdateMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }

        }

        private void AddAndShowResult(BackupStorageInfo backupStorageInfo)
        {
            try
            {
                if (backupStorageInfo.Type == BackupStorageType.NAS)
                {
                    if (_backupStorageService.ValidateNasConnection(backupStorageInfo.Path.Substring(0, backupStorageInfo.Path.Length - 1), backupStorageInfo.Username, backupStorageInfo.Password, backupStorageInfo.Domain))
                    {
                        // ekleme yap
                        var result = _backupStorageService.AddBackupStorage(backupStorageInfo);
                        if (result)
                        {
                            Close();
                            MessageBox.Show(Resources["addSuccessMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Information);
                        }
                        else
                            MessageBox.Show(Resources["addFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                    else
                    {
                        MessageBox.Show(Resources["incorrectNASMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
                else
                {
                    // kayıt et
                    var result = _backupStorageService.AddBackupStorage(backupStorageInfo);
                    if (result)
                    {
                        Close();
                        MessageBox.Show(Resources["addSuccessMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);

                    }
                    else
                        MessageBox.Show(Resources["addFailMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            catch (Exception ex)
            {
                _logger.Error(ex, "Beklenmedik hatadan dolayı ekleme işlemi başarısız oldu.");
                MessageBox.Show(Resources["unexpectedUpdateMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
            }

        }

        private void btnValidateConnection_Click(object sender, RoutedEventArgs e)
        {
            BackupStorageInfo backupStorageInfo = new BackupStorageInfo
            {
                Path = txtSettingsNASFolderPath.Text + @"\",
                Domain = txtSettingsNASDomain.Text,
                Username = txtSettingsNASUserName.Text,
                Password = txtSettingsNASPassword.Password
            };

            try
            {
                if (_backupStorageService.ValidateNasConnection(backupStorageInfo.Path.Substring(0, backupStorageInfo.Path.Length - 1), backupStorageInfo.Username, backupStorageInfo.Password, backupStorageInfo.Domain))
                {
                    //doğrulama başarılı
                    imgValidateConnectionFalse.Visibility = Visibility.Collapsed;
                    imgValidateConnectionTrue.Visibility = Visibility.Visible;
                }
                else
                {
                    //başarısız
                    imgValidateConnectionFalse.Visibility = Visibility.Visible;
                    imgValidateConnectionTrue.Visibility = Visibility.Collapsed;
                }
            }
            catch
            {
                //başarısız
                imgValidateConnectionFalse.Visibility = Visibility.Visible;
                imgValidateConnectionTrue.Visibility = Visibility.Collapsed;
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
    }
}
