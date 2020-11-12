using Autofac;
using Autofac.Core;
using Scheduler;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using WpfDIIntegration.Component;
using WpfDIIntegration.Model;
using WpfDIIntegration.Services;

namespace WpfDIIntegration
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        private IContainer _container;
        protected override void OnStartup(StartupEventArgs e)
        {
            CreateContainer();
            var mainWindow = _container.Resolve<MainWindow>();
            var tm1 = _container.Resolve<TaskManager>(new TypedParameter(typeof(Person), new Person { Name = "ali" }));
            var tm2 = _container.Resolve<TaskManager>();
            _ = tm1.GetPersonName();
            mainWindow.ShowDialog();
        }

        private void CreateContainer()
        {
            var builder = new ContainerBuilder();
            builder.RegisterType<NewMyService>().As<IMyService>();
            builder.RegisterType<OtherService>().As<IOtherService>();
            builder.RegisterType<MainWindow>();
            builder.RegisterType<SubWindow>();
            builder.RegisterType<MyJobFactory>().As(typeof(MyJobFactory).GetInterface("IJobFactory"));
            builder.RegisterType<ScheduleService>();
            builder.RegisterType<TaskRepository>().As<ITaskRepository>();
            builder.RegisterType<TaskManager>();
            _container = builder.Build();
        }
    }
}
