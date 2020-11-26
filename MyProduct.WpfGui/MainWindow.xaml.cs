using MyProduct.Business;
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
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace MyProduct.WpfGui
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private readonly ICustomerService _customerService;
        private readonly IProductService _productService;
        public MainWindow(ICustomerService customerService, IProductService productService)
        {
            InitializeComponent();
            _customerService = customerService;
            _productService = productService;
        }

        private void getCustomerListButton_Click(object sender, RoutedEventArgs e)
        {
            customerListView.ItemsSource = _customerService.GetCustomers();
        }

        private void getProductsButton_Click(object sender, RoutedEventArgs e)
        {
            productListView.ItemsSource = _productService.GetProductList();
        }
    }
}
