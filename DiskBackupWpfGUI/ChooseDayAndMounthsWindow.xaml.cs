﻿using DiskBackup.DataAccess.Abstract;
using DiskBackupWpfGUI.Utils;
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
    // Quarz'da cron pazardan başlıyor ve başlangıç değeri 1
    public partial class ChooseDayAndMounthsWindow : Window
    {
        public static string _days = null;
        public static string _months = null;
        public string _daysOrMounths;

        private readonly IConfigurationDataDal _configurationDataDal;

        private bool _chooseFlag;

        public ChooseDayAndMounthsWindow(bool chooseFlag, IConfigurationDataDal configurationDataDal)
        {
            InitializeComponent();

            _chooseFlag = chooseFlag;
            _days = null;
            _months = null;
            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

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

        public ChooseDayAndMounthsWindow(bool chooseFlag, string daysOrMounths, bool updateControl, IConfigurationDataDal configurationDataDal)
        {
            InitializeComponent();

            _chooseFlag = chooseFlag;
            _days = null;
            _months = null;
            _daysOrMounths = daysOrMounths;
            _configurationDataDal = configurationDataDal;
            SetApplicationLanguage(_configurationDataDal.Get(x => x.Key == "lang").Value);

            //chooseFlag = true gün, false ise ay
            if (chooseFlag)
            {
                UncheckDays();
                gridDays.Visibility = Visibility.Visible;
                txtTitleBar.Text = Resources["days"].ToString();
                string[] words = daysOrMounths.Split(',');
                foreach (var word in words)
                {
                    if (Convert.ToInt32(word) == 1)
                        chbSunday.IsChecked = true;
                    if (Convert.ToInt32(word) == 2)
                        chbMonday.IsChecked = true;
                    if (Convert.ToInt32(word) == 3)
                        chbTuesday.IsChecked = true;
                    if (Convert.ToInt32(word) == 4)
                        chbWednesday.IsChecked = true;
                    if (Convert.ToInt32(word) == 5)
                        chbThursday.IsChecked = true;
                    if (Convert.ToInt32(word) == 6)
                        chbFriday.IsChecked = true;
                    if (Convert.ToInt32(word) == 7)
                        chbSaturday.IsChecked = true;
                }
            }
            else
            {
                UncheckMonths();
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

        private void UncheckDays()
        {
            chbSunday.IsChecked = false;
            chbMonday.IsChecked = false;
            chbTuesday.IsChecked = false;
            chbWednesday.IsChecked = false;
            chbThursday.IsChecked = false;
            chbFriday.IsChecked = false;
            chbSaturday.IsChecked = false;
        }

        private void UncheckMonths()
        {
            chbJanuary.IsChecked = false;
            chbFebruary.IsChecked = false;
            chbMarch.IsChecked = false;
            chbApril.IsChecked = false;
            chbMay.IsChecked = false;
            chbJune.IsChecked = false;
            chbJuly.IsChecked = false;
            chbAugust.IsChecked = false;
            chbSeptember.IsChecked = false;
            chbOctober.IsChecked = false;
            chbNovember.IsChecked = false;
            chbDecember.IsChecked = false;
        }

        #region Title Bar
        private void btnChooseDayAndMounthsClose_Click(object sender, RoutedEventArgs e)
        {
            SaveDaysAndMonths();
            if (_months != null || _days != null)
                Close();
            else
                MessageBox.Show(Resources["notNullMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
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
            SaveDaysAndMonths();
            if (_months != null || _days != null)
                Close();
            else
                MessageBox.Show(Resources["notNullMB"].ToString(), Resources["MessageboxTitle"].ToString(), MessageBoxButton.OK, MessageBoxImage.Error);
        }

        private void SaveDaysAndMonths()
        {
            //Kontrol ve kayıt işlemi yap
            if (_chooseFlag) // gün
            {
                if (chbSunday.IsChecked.Value)
                {
                    _days += 1 + ",";
                }
                if (chbMonday.IsChecked.Value)
                {
                    _days += 2 + ",";
                }
                if (chbTuesday.IsChecked.Value)
                {
                    _days += 3 + ",";
                }
                if (chbWednesday.IsChecked.Value)
                {
                    _days += 4 + ",";
                }
                if (chbThursday.IsChecked.Value)
                {
                    _days += 5 + ",";
                }
                if (chbFriday.IsChecked.Value)
                {
                    _days += 6 + ",";
                }
                if (chbSaturday.IsChecked.Value)
                {
                    _days += 7 + ",";
                }
                if (_days != null)
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
                if (_months != null)
                    _months = _months.Substring(0, _months.Length - 1);
            }
        }

        public void SetApplicationLanguage(string option)
        {
            ResourceDictionary dict = new ResourceDictionary();

            switch (option)
            {
                case "tr":
                    dict.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);
                    break;
                case "en":
                    dict.Source = new Uri("..\\Resources\\Lang\\string_eng.xaml", UriKind.Relative);
                    break;
                default:
                    dict.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);
                    break;
            }
            Resources.MergedDictionaries.Add(dict);
        }
    }
}
