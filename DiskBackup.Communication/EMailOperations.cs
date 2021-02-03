using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using Serilog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Mail;
using System.Reflection;
using System.Resources;
using System.Text;
using System.Threading.Tasks;


namespace DiskBackup.Communication
{
    public class EMailOperations : IEmailOperations
    {
        public SmtpClient smtp = new SmtpClient
        {
            Host = "mail.narbulut.com",
            Port = 587,
            UseDefaultCredentials = false,
            Credentials = new NetworkCredential("diskbackup@narbulut.com", "5(k~pQ"),
            Timeout = 20000
        };

        private readonly ILogger _logger;
        private readonly IConfigurationDataDal _configurationDataDal;

        public EMailOperations(ILogger logger, IConfigurationDataDal configurationDataDal)
        {
            _logger = logger.ForContext<EMailOperations>();
            _configurationDataDal = configurationDataDal;
        }

        public void SendMail(List<EmailInfo> emailAddresses, StatusInfo statusInfo)
        {
            var lang = _configurationDataDal.Get(x => x.Key == "lang");
            if (lang == null)
            {
                lang = new ConfigurationData { Key = "lang", Value = "tr" };

            }
            using (var message = new MailMessage("diskbackup@narbulut.com", "diskbackup@narbulut.com")
            {
                IsBodyHtml = true,
                Subject = statusInfo.TaskName, //status
                Body = ChangeBody(lang.Value, statusInfo),
                From = new MailAddress("diskbackup@narbulut.com", "Narbulut Bilgilendirme")
            })

                try
                {
                    foreach (var item in emailAddresses)
                    {
                        message.To.Add(item.EmailAddress);
                    }
                    smtp.Send(message);
                    _logger.Information("Mail gönderimi başarılı.");
                }
                catch (SmtpException ex)
                {
                    _logger.Error(ex, "Mail gönderimi Başarısız.");
                }

        }

        public string ChangeBody(string lang, StatusInfo statusInfo)
        {
            string body = string.Empty;
            if (lang == "tr")
            {
                body = ChangeLang("tr");
            }
            else
            {
                body = ChangeLang("tr");
            }
            body = body.Replace("{FileName}", statusInfo.FileName);
            body = body.Replace("{Duration}", statusInfo.TimeElapsed.ToString()); //hesaplama
            body = body.Replace("{SourceInfo}", statusInfo.SourceObje);
            body = body.Replace("{TaskName}", statusInfo.TaskName);
            body = body.Replace("{AverageDataTransfer}", Math.Round(statusInfo.AverageDataRate, 2).ToString() + " MB/s"); 
            body = body.Replace("{ProcessedData}", statusInfo.DataProcessed.ToString()); //HESAPLAMA
            body = body.Replace("{InstantDataTransfer}", Math.Round(statusInfo.InstantDataRate,2).ToString() + " MB/s");
            body = body.Replace("{txtWelcome}", DateTime.Now.ToString() + " Tarihli blabla"); // biz yazıcaz
            return body;
        }

        public string ChangeLang(string lang)
        {
            string body = string.Empty;
            using (StreamReader reader = new StreamReader(@".\EmailTemplate.html"))
            {
                body = reader.ReadToEnd();
            }

            if (lang == "en")
            {
                #region En 
                body = body.Replace("{Hello}", "Hello");
                body = body.Replace("{ListTextLang}", "The list below shows details of the task");
                body = body.Replace("{StatusInfoLang}", "Status Information");
                body = body.Replace("{TaskNameLang}", "Task Name");
                body = body.Replace("{FileNameLang}", "File Name");
                body = body.Replace("{DurationLang}", "Duration");
                body = body.Replace("{ProcessedDataLang}", "Processed Data");
                body = body.Replace("{AverageDataTransferLang}", "Average Data Transfer");
                body = body.Replace("{InstantDataTransferLang}", "Instant Data Transfer");
                body = body.Replace("{SourceInfoLang}", "Source Info");
                body = body.Replace("{RespectLang}", "Best Regards");
                body = body.Replace("{AllRightReservedLang}", "All Right Reserved");
                #endregion
            }
            else
            {
                #region TR
                body = body.Replace("{Hello}", "Merhaba");
                body = body.Replace("{ListTextLang}", "Aşağıdaki listede görevle ilgili detaylar gösterilmektedir;");
                body = body.Replace("{StatusInfoLang}", "Durum Bilgisi");
                body = body.Replace("{TaskNameLang}", "Görev Adı");
                body = body.Replace("{FileNameLang}", "Dosya Adı");
                body = body.Replace("{DurationLang}", "Süre");
                body = body.Replace("{ProcessedDataLang}", "İşlenen Veri");
                body = body.Replace("{AverageDataTransferLang}", "Ortalama Veri Aktarımı");
                body = body.Replace("{InstantDataTransferLang}", "Anlık Veri Aktarımı");
                body = body.Replace("{SourceInfoLang}", "Kaynak Bilgisi");
                body = body.Replace("{RespectLang}", "Saygılarımızla");
                body = body.Replace("{AllRightReservedLang}", "Tüm Hakları Saklıdır");
                #endregion
            }
            return body;
        }
    }
}

