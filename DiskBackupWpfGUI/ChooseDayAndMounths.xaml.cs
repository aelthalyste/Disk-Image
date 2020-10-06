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
    /// Interaction logic for ChooseDayAndMounths.xaml
    /// </summary>
    public partial class ChooseDayAndMounths : Window
    {
        public ChooseDayAndMounths(bool chooseFlag)
        {
            InitializeComponent();
            //chooseFlag = true gün, false ise ay
            if (chooseFlag)
            {
                gridDays.Visibility = Visibility.Visible;
                txtTitleBar.Text = Resources["days"].ToString();
            }
            else
            {
                gridMounths.Visibility = Visibility.Visible;
                txtTitleBar.Text = Resources["mounths"].ToString();
            }
        }

        #region Title Bar
        private void btnChooseDayAndMounthsClose_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void MyTitleBar_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }
        #endregion

        private void btnSave_Click(object sender, RoutedEventArgs e)
        {
            //Kontrol ve kayıt işlemi yap
            //instance almak mı yoksa buton değiştirmek mi?
            Close();
        }
    }
}
