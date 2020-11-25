using Autofac;
using MyProduct.Business;
using MyProduct.Entities.Repositories;
using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.ServiceModel;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace MyProduct.Service
{
    public partial class MyProductService : ServiceBase
    {
        private static IContainer _container;
        private ServiceHost _customerHost;
        private ServiceHost _productHost;
        public MyProductService()
        {
            InitializeComponent();
        }

        protected override void OnStart(string[] args)
        {
            var builder = new ContainerBuilder();
            builder.RegisterType<CustomerService>().As<ICustomerService>().SingleInstance();
            builder.RegisterType<ProductService>().As<IProductService>().SingleInstance();
            builder.RegisterType<CustomerRepository>().As<ICustomerRepository>();
            _container = builder.Build();
            var customerService = _container.Resolve<ICustomerService>();
            var productService = _container.Resolve<IProductService>();
            Uri baseAddress = new Uri("net.pipe://localhost/myproduct");
            _customerHost = new ServiceHost(customerService, baseAddress);
            _customerHost.AddServiceEndpoint(typeof(ICustomerService), new NetNamedPipeBinding(), "customerservice");
            _productHost = new ServiceHost(productService, baseAddress);
            _productHost.AddServiceEndpoint(typeof(IProductService), new NetNamedPipeBinding(), "productservice");
            _customerHost.Open();
            _productHost.Open();
        }

        protected override void OnStop()
        {
            _customerHost = null;
            _customerHost?.Close();
            _productHost?.Close();
        }
    }
}
