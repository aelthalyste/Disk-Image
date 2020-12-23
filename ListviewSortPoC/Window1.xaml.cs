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
    }

    public class Product
    {
        public string Name { get; set; }
        public float Price { get; set; }
        public string Origin { get; set; }
        public DateTime ExpirationDate { get; set; }
    }
}
