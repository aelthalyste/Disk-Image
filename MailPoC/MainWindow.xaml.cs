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
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Net.Mail;
using System.Net;

namespace MailPoC
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void btnReset_Click(object sender, RoutedEventArgs e)
        {
            txtTo.Text = "";
            txtContent.Text = "";
            txtSubject.Text = "";
        }

        private void btnSend_Click(object sender, RoutedEventArgs e)
        {
            var smtp = new SmtpClient
            {
                Host = "mail.narbulut.com",
                Port = 587,
                UseDefaultCredentials = false,
                Credentials = new NetworkCredential("diskbackup@narbulut.com", "5(k~pQ"),
                Timeout = 20000
            };

            using (var message = new MailMessage("diskbackup@narbulut.com", txtTo.Text)
            {
                Subject = txtSubject.Text,
                Body = txtContent.Text
            })

                try
                {
                    smtp.Send(message);
                    MessageBox.Show("eposta gönderimi başarılı");
                }
                catch (SmtpException ex)
                {
                    MessageBox.Show("başarısız" + ex);
                }

            //MailMessage ePosta = new MailMessage();
            //ePosta.From = new MailAddress("diskbackup@narbulut.com");
            //ePosta.To.Add("eyup.katirci@bilisimcenter.com");
            //ePosta.To.Add("ebru.vural@bilisimcenter.com");
            //ePosta.Subject = "Narbulut Bilgilendirme";
            //ePosta.Body = "sketch book v3";

            //SmtpClient smtp = new SmtpClient();
            //smtp.Credentials = new System.Net.NetworkCredential("diskbackup@narbulut.com", "5(k~pQ");
            //smtp.Port = 587;
            //smtp.Host = "mail.narbulut.com";
        }
    }
}
