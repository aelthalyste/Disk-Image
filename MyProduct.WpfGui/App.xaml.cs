using Autofac;
using MyProduct.Business;
using MyProduct.Entities.Repositories;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;

namespace MyProduct.WpfGui
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        private static IContainer _container;
        protected override void OnStartup(StartupEventArgs e)
        {
            var containerBuilder = new ContainerBuilder();
            containerBuilder.Register(c => new CustomerServiceClient().CustomerService);
            containerBuilder.Register(c => new ProductServiceClient().ProductService);
            containerBuilder.RegisterType<MainWindow>();
            _container = containerBuilder.Build();
            var window = _container.Resolve<MainWindow>();
            window.Show();
        }
    }
}
