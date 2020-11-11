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
    public partial class ChooseDayAndMounthsWindow : Window
    {
        public static string _days = null;
        public static string _months = null;

        private bool _chooseFlag;

        public ChooseDayAndMounthsWindow(bool chooseFlag)
        {
            InitializeComponent();

            _chooseFlag = chooseFlag;
            _days = null;
            _months = null;

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

        public ChooseDayAndMounthsWindow(bool chooseFlag, string daysOrMounths)
        {
            InitializeComponent();

            _chooseFlag = chooseFlag;
            _days = null;
            _months = null;

            //chooseFlag = true gün, false ise ay
            if (chooseFlag)
            {
                gridDays.Visibility = Visibility.Visible;
                txtTitleBar.Text = Resources["days"].ToString();
                string[] words = daysOrMounths.Split(',');
                foreach (var word in words)
                {
                    if (Convert.ToInt32(word) == 0)
                        chbMonday.IsChecked = true;
                    if (Convert.ToInt32(word) == 1)
                        chbTuesday.IsChecked = true;
                    if (Convert.ToInt32(word) == 2)
                        chbWednesday.IsChecked = true;
                    if (Convert.ToInt32(word) == 3)
                        chbThursday.IsChecked = true;
                    if (Convert.ToInt32(word) == 4)
                        chbFriday.IsChecked = true;
                    if (Convert.ToInt32(word) == 5)
                        chbSaturday.IsChecked = true;
                    if (Convert.ToInt32(word) == 6)
                        chbSunday.IsChecked = true;
                }
            }
            else
            {
                gridMounths.Visibility = Visibility.Visible;
                txtTitleBar.Text = Resources["mounths"].ToString();
                string[] words = daysOrMounths.Split(',');
                foreach (var word in words)
                {
                    if (Convert.ToInt32(word) == 1)
                        chbJanuary.IsChecked = true;
                    if (Convert.ToInt32(word) == 2)
                        chbFebruary.IsChecked = true;
                    if (Convert.ToInt32(word) == 3)
                        chbMarch.IsChecked = true;
                    if (Convert.ToInt32(word) == 4)
                        chbApril.IsChecked = true;
                    if (Convert.ToInt32(word) == 5)
                        chbMay.IsChecked = true;
                    if (Convert.ToInt32(word) == 6)
                        chbJune.IsChecked = true;
                    if (Convert.ToInt32(word) == 7)
                        chbJuly.IsChecked = true;
                    if (Convert.ToInt32(word) == 8)
                        chbAugust.IsChecked = true;
                    if (Convert.ToInt32(word) == 9)
                        chbSeptember.IsChecked = true;
                    if (Convert.ToInt32(word) == 10)
                        chbOctober.IsChecked = true;
                    if (Convert.ToInt32(word) == 11)
                        chbNovember.IsChecked = true;
                    if (Convert.ToInt32(word) == 12)
                        chbDecember.IsChecked = true;
                }
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
            if (_chooseFlag) // gün
            {
                if (chbMonday.IsChecked.Value)
                {
                    _days += 0 + ",";
                }
                if (chbTuesday.IsChecked.Value)
                {
                    _days += 1 + ",";
                }
                if (chbWednesday.IsChecked.Value)
                {
                    _days += 2 + ",";
                }
                if (chbThursday.IsChecked.Value)
                {
                    _days += 3 + ",";
                }
                if (chbFriday.IsChecked.Value)
                {
                    _days += 4 + ",";
                }
                if (chbSaturday.IsChecked.Value)
                {
                    _days += 5 + ",";
                }
                if (chbSunday.IsChecked.Value)
                {
                    _days += 6 + ",";
                }
                _days = _days.Substring(0, _days.Length - 1);
            }
            else
            {
                if (chbJanuary.IsChecked.Value)
                {
                    _months += 1 + ",";
                }
                if (chbFebruary.IsChecked.Value)
                {
                    _months += 2 + ",";
                }
                if (chbMarch.IsChecked.Value)
                {
                    _months += 3 + ",";
                }
                if (chbApril.IsChecked.Value)
                {
                    _months += 4 + ",";
                }
                if (chbMay.IsChecked.Value)
                {
                    _months += 5 + ",";
                }
                if (chbJune.IsChecked.Value)
                {
                    _months += 6 + ",";
                }
                if (chbJuly.IsChecked.Value)
                {
                    _months += 7 + ",";
                }
                if (chbAugust.IsChecked.Value)
                {
                    _months += 8 + ",";
                }
                if (chbSeptember.IsChecked.Value)
                {
                    _months += 9 + ",";
                }
                if (chbOctober.IsChecked.Value)
                {
                    _months += 10 + ",";
                }
                if (chbNovember.IsChecked.Value)
                {
                    _months += 11 + ",";
                }
                if (chbDecember.IsChecked.Value)
                {
                    _months += 12 + ",";
                }
                _months = _months.Substring(0, _months.Length - 1);
            }
            Close();
        }
    }
}
