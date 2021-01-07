using System;
using System.Collections.Generic;
using System.ComponentModel;
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

namespace ListviewSortPoC
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private GridViewColumnHeader listViewSortCol = null;
        private SortAdorner listViewSortAdorner = null;

        private GridViewColumnHeader listViewSortCol2 = null;
        private SortAdorner listViewSortAdorner2 = null;

        public MainWindow()
        {
            InitializeComponent();
            //SetApplicationLanguage("tr");
            denemeConfig denemeConfig = new denemeConfig();
            if (denemeConfig.GetConfig("lang")== null)
            {
                denemeConfig.SetConfig("lang", "tr");
                MessageBox.Show("Türkçe yaptım");
            }
            if (denemeConfig.GetConfig("lang") == "tr")
            {
                MessageBox.Show("tr geldi");
                SetApplicationLanguage("tr");
            }
            else
            {
                MessageBox.Show("en geldi");
                SetApplicationLanguage("en");
            }
            List<User> items = new List<User>();
            items.Add(new User() { Name = "21.12.2020 15:10", Age = 42, Sex = SexType.Male, Date = DateTime.Now-TimeSpan.FromDays(10) });
            items.Add(new User() { Name = "22.12.2019 15:10", Age = 39, Sex = SexType.Female, Date = DateTime.Now - TimeSpan.FromDays(5) });
            items.Add(new User() { Name = "20.12.2018 15:10", Age = 13, Sex = SexType.Male, Date = DateTime.Now - TimeSpan.FromDays(100) });
            items.Add(new User() { Name = "01.12.2025 15:10", Age = 13, Sex = SexType.Female, Date = DateTime.Now - TimeSpan.FromDays(1) });
            lvUsers.ItemsSource = items;

            List<User2> items2 = new List<User2>();
            items2.Add(new User2() { Name = "21.12.2020 15:10", Age = 42, Sex = SexType.Male, Date = DateTime.Now - TimeSpan.FromDays(10) });
            items2.Add(new User2() { Name = "22.12.2019 15:10", Age = 39, Sex = SexType.Female, Date = DateTime.Now - TimeSpan.FromDays(5) });
            items2.Add(new User2() { Name = "20.12.2018 15:10", Age = 13, Sex = SexType.Male, Date = DateTime.Now - TimeSpan.FromDays(100) });
            items2.Add(new User2() { Name = "01.12.2025 15:10", Age = 13, Sex = SexType.Female, Date = DateTime.Now - TimeSpan.FromDays(1) });
            lvUsers2.ItemsSource = items2;
        }

        public void SetApplicationLanguage(string option)
        {
            ResourceDictionary dict = new ResourceDictionary();

            switch (option)
            {
                case "tr":
                    dict.Source = new Uri("..\\Resources\\string_tr.xaml", UriKind.Relative);
                    break;
                case "en":
                    dict.Source = new Uri("..\\Resources\\string_eng.xaml", UriKind.Relative);
                    break;
                default:
                    dict.Source = new Uri("..\\Resources\\string_tr.xaml", UriKind.Relative);
                    break;
            }
            Resources.MergedDictionaries.Add(dict);
        }

        private void lvUsersColumnHeader_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewSortCol != null)
            {
                AdornerLayer.GetAdornerLayer(listViewSortCol).Remove(listViewSortAdorner);
                lvUsers.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewSortCol == column && listViewSortAdorner.Direction == newDir)
                newDir = ListSortDirection.Descending;

            listViewSortCol = column;
            listViewSortAdorner = new SortAdorner(listViewSortCol, newDir);
            AdornerLayer.GetAdornerLayer(listViewSortCol).Add(listViewSortAdorner);
            lvUsers.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            Window1 window1 = new Window1();
            window1.Show();
        }

        private void Button_Click_1(object sender, RoutedEventArgs e)
        {
            Window2 window2 = new Window2();
            window2.Show();
        }

        private void GridViewColumnHeader_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewSortCol2 != null)
            {
                AdornerLayer.GetAdornerLayer(listViewSortCol2).Remove(listViewSortAdorner2);
                lvUsers2.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewSortCol2 == column && listViewSortAdorner2.Direction == newDir)
                newDir = ListSortDirection.Descending;

            listViewSortCol2 = column;
            listViewSortAdorner2 = new SortAdorner(listViewSortCol2, newDir);
            AdornerLayer.GetAdornerLayer(listViewSortCol2).Add(listViewSortAdorner2);
            lvUsers2.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));
        }

        private void eng_Click(object sender, RoutedEventArgs e)
        {
            denemeConfig denemeConfig = new denemeConfig();
            denemeConfig.SetConfig("lang", "en");
            SetApplicationLanguage("en");
        }

        private void tr_Click(object sender, RoutedEventArgs e)
        {
            denemeConfig denemeConfig = new denemeConfig();
            denemeConfig.SetConfig("lang", "tr");
            SetApplicationLanguage("tr");
        }

        //private void cbLang_SelectionChanged(object sender, SelectionChangedEventArgs e)
        //{
        //    if (cbLang.SelectedIndex != -1)
        //    {
        //        ResourceDictionary dict = new ResourceDictionary();
        //        if (cbLang.SelectedIndex == 0) // Türkçe
        //        {
        //            dict.Source = new Uri("..\\Resources\\strings_tr.xaml", UriKind.Relative);
        //            Resources.MergedDictionaries.Add(dict);
        //        }
        //        else if (cbLang.SelectedIndex == 1) // ingilizce
        //        {
        //            dict.Source = new Uri("..\\Resources\\strings_eng.xaml", UriKind.Relative);
        //            Resources.MergedDictionaries.Add(dict);
        //        }
        //    }
        //}
    }

    public enum SexType { Male, Female };

	public class User
	{
		public string Name { get; set; }
		public int Age { get; set; }
		public string Mail { get; set; }
		public SexType Sex { get; set; }
		public DateTime Date { get; set; }
	}

    public class User2
    {
        public string Name { get; set; }
        public int Age { get; set; }
        public string Mail { get; set; }
        public SexType Sex { get; set; }
        public DateTime Date { get; set; }
    }
}
