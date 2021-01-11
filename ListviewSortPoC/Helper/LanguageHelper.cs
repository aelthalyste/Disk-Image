using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace ListviewSortPoC.Helper
{
    public class LanguageHelper
    {
        public string lang { get; set; } = "tr";
        public ResourceDictionary SetApplicationLanguage(string option)
        {
            lang = option;
            ResourceDictionary dict = new ResourceDictionary();

            switch (lang)
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
            return dict;
        }

        public void ChangeAppLang(string option)
        {
            lang = option;
        }
    }
}
