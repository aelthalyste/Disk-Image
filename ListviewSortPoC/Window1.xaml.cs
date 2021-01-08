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
using System.Windows.Shapes;

namespace ListviewSortPoC
{
    /// <summary>
    /// Interaction logic for Window1.xaml
    /// </summary>
    public partial class Window1 : Window
    {
        private GridViewColumnHeader listViewSortCol = null;
        private SortAdorner listViewSortAdorner = null;

        public Window1()
        {
            InitializeComponent();
            denemeConfig denemeConfig = new denemeConfig();
            if (denemeConfig.GetConfig("lang") == null)
            {
                denemeConfig.SetConfig("lang", "tr");
                SetApplicationLanguage("tr");
            }
            else
            {
                SetApplicationLanguage(denemeConfig.GetConfig("lang"));
            }
            List<Product> items = new List<Product>();
            items.Add(new Product() { Name = "!Pencil", Price = 42, Origin = "Turkey", ExpirationDate = DateTime.Now - TimeSpan.FromDays(10) });
            items.Add(new Product() { Name = "-Pencil", Price = 39, Origin = "France", ExpirationDate = DateTime.Now - TimeSpan.FromDays(5) });
            items.Add(new Product() { Name = "@Pencil", Price = 13, Origin = "Spain", ExpirationDate = DateTime.Now - TimeSpan.FromDays(100) });
            items.Add(new Product() { Name = "*Pencil", Price = 13, Origin = "Turkey", ExpirationDate = DateTime.Now - TimeSpan.FromDays(1) });
            lvUsers.ItemsSource = items;
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
    }



    public class Product
    {
        public string Name { get; set; }
        public float Price { get; set; }
        public string Origin { get; set; }
        public DateTime ExpirationDate { get; set; }
    }
}
