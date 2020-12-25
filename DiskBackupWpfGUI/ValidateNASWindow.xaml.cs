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
    /// Interaction logic for ValidateNAS.xaml
    /// </summary>
    public partial class ValidateNASWindow : Window
    {
        public ValidateNASWindow()
        {
            InitializeComponent();
        }
        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }

        private void btnValidateNAS_Click(object sender, RoutedEventArgs e)
        {

        }

        private void btnValidateNASClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
