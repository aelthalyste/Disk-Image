using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace MailPoC
{
    public class MailDenemeClass
    {
        public static string ChangeLang(string lang)
        {
            string body = string.Empty;
            using (StreamReader reader = new StreamReader(@".\EmailTemplate.html"))
            {
                body = reader.ReadToEnd();
            }
            if (lang == "en")
                Application.Current.Resources.Source = new Uri("..\\Resources\\Lang\\string_eng.xaml", UriKind.Relative);
            else
                Application.Current.Resources.Source = new Uri("..\\Resources\\Lang\\string_tr.xaml", UriKind.Relative);

            body = body.Replace("{Hello}", Application.Current.Resources["hello"].ToString());
            body = body.Replace("{ListTextLang}", Application.Current.Resources["listTextLang"].ToString());
            body = body.Replace("{StatusInfoLang}", Application.Current.Resources["statusInfo"].ToString());
            body = body.Replace("{TaskNameLang}", Application.Current.Resources["taskName"].ToString());
            body = body.Replace("{FileNameLang}", Application.Current.Resources["fileName"].ToString());
            body = body.Replace("{DurationLang}", Application.Current.Resources["duration"].ToString());
            body = body.Replace("{ProcessedDataLang}", Application.Current.Resources["processedData"].ToString());
            body = body.Replace("{AverageDataTransferLang}", Application.Current.Resources["averageDataTransfer"].ToString());
            body = body.Replace("{InstantDataTransferLang}", Application.Current.Resources["instantDataTransfer"].ToString());
            body = body.Replace("{SourceInfoLang}", Application.Current.Resources["sourceInfo"].ToString());
            body = body.Replace("{RespectLang}", Application.Current.Resources["respectLang"].ToString());
            body = body.Replace("{AllRightReservedLang}", Application.Current.Resources["allRightReserved"].ToString());
            return body;
        }


        public static string ChangeBody()
        {
            string body = ChangeLang("tr");
            body = body.Replace("{FileName}", "eyüp");
            body = body.Replace("{Duration}", "ebru");
            body = body.Replace("{SourceInfo}", "12123");
            body = body.Replace("{TaskName}", "C BACKUPPPPPP XD XD ");
            body = body.Replace("{AverageDataTransfer}", "12 MB/s");
            body = body.Replace("{ProcessedData}", "52 MB/s");
            body = body.Replace("{InstantDataTransfer}", "42 MB/s");
            body = body.Replace("{txtWelcome}", DateTime.Now.ToString() + "Tarihli blabla");
            return body;
        }
    }
}
