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
    /// Interaction logic for Window2.xaml
    /// </summary>
    public partial class Window2 : Window
    {
        private GridViewColumnHeader listViewSortCol = null;
        private SortAdorner listViewSortAdorner = null;

        public Window2()
        {
            InitializeComponent();
            List<Backup> items = new List<Backup>();
            items.Add(new Backup() { FileName = "21.12.2020 15:10", StrVolumeSize = "12 GB", FileSize = 4, StrFileSize = "11 GB", CreatedDate = DateTime.Now - TimeSpan.FromDays(10) });
            items.Add(new Backup() { FileName = "22.12.2019 15:10", StrVolumeSize = "15 GB", FileSize = 3, StrFileSize = "10 GB", CreatedDate = DateTime.Now - TimeSpan.FromDays(5) });
            items.Add(new Backup() { FileName = "20.12.2018 15:10", StrVolumeSize = "16 GB", FileSize = 2, StrFileSize = "9 GB", CreatedDate = DateTime.Now - TimeSpan.FromDays(100) });
            items.Add(new Backup() { FileName = "01.12.2025 15:10", StrVolumeSize = "17 GB", FileSize = 1, StrFileSize = "9,5 GB", CreatedDate = DateTime.Now - TimeSpan.FromDays(1) });
            listViewRestore.ItemsSource = items;
        }

        private void lvUsersColumnHeader_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = (sender as GridViewColumnHeader);
            string sortBy = column.Tag.ToString();
            if (listViewSortCol != null)
            {
                AdornerLayer.GetAdornerLayer(listViewSortCol).Remove(listViewSortAdorner);
                listViewRestore.Items.SortDescriptions.Clear();
            }

            ListSortDirection newDir = ListSortDirection.Ascending;
            if (listViewSortCol == column && listViewSortAdorner.Direction == newDir)
                newDir = ListSortDirection.Descending;

            listViewSortCol = column;
            listViewSortAdorner = new SortAdorner(listViewSortCol, newDir);
            AdornerLayer.GetAdornerLayer(listViewSortCol).Add(listViewSortAdorner);
            listViewRestore.Items.SortDescriptions.Add(new SortDescription(sortBy, newDir));
        }
    }
    public class Backup
    {
        public string FileName { get; set; }
        public string StrVolumeSize { get; set; }
        public int FileSize { get; set; }
        public string StrFileSize { get; set; }
        public DateTime CreatedDate { get; set; }
    }
}
