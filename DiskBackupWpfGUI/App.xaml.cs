using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.TaskScheduler;
using DiskBackup.TaskScheduler.Factory;
using DiskBackup.TaskScheduler.Jobs;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
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
            var builder= new ContainerBuilder();
            builder.RegisterType<EfRestoreTaskDal>().As<IRestoreTaskDal>();
            builder.RegisterType<EfActivityLogDal>().As<IActivityLogDal>();
            builder.RegisterType<EfBackupStorageDal>().As<IBackupStorageDal>();
            builder.RegisterType<EfBackupTaskDal>().As<IBackupTaskDal>();
            builder.RegisterType<EfStatusInfoDal>().As<IStatusInfoDal>();
            builder.RegisterType<EfTaskInfoDal>().As<ITaskInfoDal>();
            builder.RegisterType<BackupService>().As<IBackupService>().SingleInstance();
            builder.RegisterType<BackupStorageService>().As<IBackupStorageService>();
            builder.RegisterType<LogService>().As<ILogService>();
            builder.RegisterType<TaskSchedulerManager>().As<ITaskSchedulerManager>().SingleInstance();
            builder.RegisterType<BackupFullJob>();
            builder.RegisterType<BackupIncDiffJob>();
            builder.RegisterType<RestoreDiskJob>();
            builder.RegisterType<RestoreVolumeJob>();
            builder.RegisterType<DiskBackupJobFactory>().As(typeof(DiskBackupJobFactory).GetInterface("IJobFactory"));
            builder.RegisterType<MainWindow>();
            builder.RegisterType<NewCreateTaskWindow>();
            builder.RegisterType<RestoreWindow>();
            builder.RegisterType<StatusesWindow>();
            builder.RegisterType<AddBackupAreaWindow>();
            builder.RegisterType<FileExplorerWindow>();
            builder.RegisterType<ChooseDayAndMounthsWindow>();
            _container = builder.Build();
        }
    }
}
