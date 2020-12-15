using Autofac;
using DiskBackup.Business.Abstract;
using DiskBackup.Business.Concrete;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.DataAccess.Concrete.EntityFramework;
using DiskBackup.Entities.Concrete;
using DiskBackup.TaskScheduler;
using DiskBackup.TaskScheduler.Factory;
using DiskBackup.TaskScheduler.Jobs;
using Quartz.Spi;
using Serilog;
using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.ServiceModel;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Service
{
    public partial class DiskBackupService : ServiceBase
    {
        private static IContainer _container;
        private ServiceHost _backupServiceHost;
        private ServiceHost _backupStorageServiceHost;
        private ServiceHost _logServiceHost;
        private ServiceHost _taskSchedulerHost;
        public DiskBackupService()
        {
            InitializeComponent();
        }

        public void CreateContainer()
        {
            var logger = new LoggerConfiguration()
                .Destructure.ByTransforming<TaskInfo>(t => "|" + t.Id + "_" + t.Name + "|")
                .MinimumLevel.Information()
                .WriteTo.File(Assembly.GetExecutingAssembly().Location + ".logs.txt", flushToDiskInterval: TimeSpan.FromMilliseconds(300),
                    outputTemplate: "{Timestamp:yyyy-MM-dd HH:mm:ss} [{Level:u}] {Properties:l} {Message:l}{NewLine}{Exception}")
                .CreateLogger();

            var builder = new ContainerBuilder();
            builder.RegisterInstance(logger).As<ILogger>().SingleInstance();
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
            builder.RegisterType<DiskBackupJobFactory>().As<IJobFactory>();
            _container = builder.Build();

        }

        protected override void OnStart(string[] args)
        {
            CreateContainer();
            CleanUp();
            StartHost();
        }

        private void CleanUp()
        {

        }

        private void StartHost()
        {
            var backupService = _container.Resolve<IBackupService>();
            var backupStorageService = _container.Resolve<IBackupStorageService>();
            var logService = _container.Resolve<ILogService>();
            var taskScheduler = _container.Resolve<ITaskSchedulerManager>();
            backupService.InitTracker();
            taskScheduler.InitShedulerAsync().Wait();
            Uri baseAddress = new Uri("net.pipe://localhost/nardiskbackup");
            _backupServiceHost = new ServiceHost(backupService, baseAddress);
            _backupServiceHost.AddServiceEndpoint(typeof(IBackupService), new NetNamedPipeBinding() { SendTimeout = TimeSpan.MaxValue, ReceiveTimeout = TimeSpan.MaxValue, CloseTimeout = TimeSpan.FromSeconds(2) }, "backupservice");
            _backupStorageServiceHost = new ServiceHost(backupStorageService, baseAddress);
            _backupStorageServiceHost.AddServiceEndpoint(typeof(IBackupStorageService), new NetNamedPipeBinding() { SendTimeout = TimeSpan.MaxValue, ReceiveTimeout = TimeSpan.MaxValue, CloseTimeout = TimeSpan.FromSeconds(2) }, "backupstorageservice");
            _logServiceHost = new ServiceHost(logService, baseAddress);
            _logServiceHost.AddServiceEndpoint(typeof(ILogService), new NetNamedPipeBinding() { SendTimeout = TimeSpan.MaxValue, ReceiveTimeout = TimeSpan.MaxValue, CloseTimeout = TimeSpan.FromSeconds(2) }, "logservice");
            _taskSchedulerHost = new ServiceHost(taskScheduler, baseAddress);
            _taskSchedulerHost.AddServiceEndpoint(typeof(ITaskSchedulerManager), new NetNamedPipeBinding() { SendTimeout = TimeSpan.MaxValue, ReceiveTimeout = TimeSpan.MaxValue, CloseTimeout = TimeSpan.FromSeconds(2) }, "taskscheduler");
            _backupServiceHost.Open();
            _backupStorageServiceHost.Open();
            _logServiceHost.Open();
            _taskSchedulerHost.Open();
        }

        protected override void OnStop()
        {
            _backupServiceHost.Close();
            _backupStorageServiceHost.Close();
            _logServiceHost.Close();
            _taskSchedulerHost.Close();
        }
    }
}
