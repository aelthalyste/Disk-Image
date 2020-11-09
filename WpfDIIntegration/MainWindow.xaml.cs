using Autofac;
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
using WpfDIIntegration.Services;

namespace WpfDIIntegration
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private readonly IMyService _myService;
        private readonly IOtherService _otherService;
        private readonly ILifetimeScope _scope;

        public MainWindow(IMyService myService, IOtherService otherService, ILifetimeScope scope)
        {
            InitializeComponent();
            _myService = myService;
            _otherService = otherService;
            _scope = scope;
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show(_otherService.Cities().First());
            _myService.ScheduleTask();
            personListView.ItemsSource = _myService.GetPeople();
        } 

        private void Button_Click_1(object sender, RoutedEventArgs e)
        {
            var subWin = _scope.Resolve<SubWindow>();
            subWin.ShowDialog();
        }
    }
}
