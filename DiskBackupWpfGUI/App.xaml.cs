using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.Communication;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.TaskScheduler;
using DiskBackup.TaskScheduler.Factory;
using DiskBackup.TaskScheduler.Jobs;
using DiskBackupWpfGUI.Utils;
using Serilog;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;

namespace DiskBackupWpfGUI
{    
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        private static IContainer _container;

        protected override void OnStartup(StartupEventArgs e)
        {
            CreateContainer();
            var mainWindow = _container.Resolve<MainWindow>();
            mainWindow.Show();
        }

        private void CreateContainer()
        {
            var logger = new LoggerConfiguration()
                .MinimumLevel.Information()
                .WriteTo.File(Assembly.GetExecutingAssembly().Location + ".logs.txt", flushToDiskInterval: TimeSpan.FromMilliseconds(300),
                    outputTemplate: "{Timestamp:yyyy-MM-dd HH:mm:ss} [{Level:u}] {Properties:l} {Message:l}{NewLine}{Exception}")
                .CreateLogger();
            var builder= new ContainerBuilder();
            builder.RegisterInstance(logger).As<ILogger>().SingleInstance();
            builder.RegisterType<EfRestoreTaskDal>().As<IRestoreTaskDal>();
            builder.RegisterType<EfActivityLogDal>().As<IActivityLogDal>();
            builder.RegisterType<EfBackupStorageDal>().As<IBackupStorageDal>();
            builder.RegisterType<EfBackupTaskDal>().As<IBackupTaskDal>();
            builder.RegisterType<EfStatusInfoDal>().As<IStatusInfoDal>();
            builder.RegisterType<EfTaskInfoDal>().As<ITaskInfoDal>();
            builder.RegisterType<EfConfigurationDataDal>().As<IConfigurationDataDal>();
            builder.RegisterType<EfEmailInfoDal>().As<IEmailInfoDal>();
            builder.Register(c => new BackupServiceClient().BackupService);
            builder.Register(c => new BackupStorageServiceClient().BackupStorageService);
            builder.Register(c => new LogServiceClient().LogService);
            builder.Register(c => new TaskSchedulerClient().TaskScheduler);
            builder.RegisterType<MainWindow>();
            builder.RegisterType<NewCreateTaskWindow>();
            builder.RegisterType<RestoreWindow>();
            builder.RegisterType<StatusesWindow>();
            builder.RegisterType<AddBackupAreaWindow>();
            builder.RegisterType<FileExplorerWindow>();
            builder.RegisterType<ChooseDayAndMounthsWindow>();
            builder.RegisterType<ValidateNASWindow>();
            builder.RegisterType<LicenseControllerWindow>();
            builder.RegisterType<EMailSettingsWindow>();
            builder.RegisterType<EMailOperations>().As<IEMailOperations>();
            _container = builder.Build();
        }
    }
}
