using Autofac;
using Autofac.Core;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
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
            mainWindow.ShowDialog();
        }

        private void CreateContainer()
        {
            var builder = new ContainerBuilder();
            builder.RegisterType<NewMyService>().As<IMyService>();
            builder.RegisterType<OtherService>().As<IOtherService>();
            builder.RegisterType<MainWindow>();
            builder.RegisterType<SubWindow>();
            _container = builder.Build();
        }
    }
}
