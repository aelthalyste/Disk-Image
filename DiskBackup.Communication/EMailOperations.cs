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
    public class EMailOperations : IEMailOperations
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
        private readonly IEmailInfoDal _emailInfoDal;

        public EMailOperations(ILogger logger, IConfigurationDataDal configurationDataDal, IEmailInfoDal emailInfoDal)
        {
            _logger = logger.ForContext<EMailOperations>();
            _configurationDataDal = configurationDataDal;
            _emailInfoDal = emailInfoDal;
        }

        private void EMailSender(List<EmailInfo> emailAddresses, StatusInfo statusInfo)
        {
            ConfigurationData lang = GetLang();
            if (lang == null)
                lang = new ConfigurationData{ Key = "lang", Value = "tr" };

            using (var message = new MailMessage("diskbackup@narbulut.com", "diskbackup@narbulut.com")
            {
                IsBodyHtml = true,
                Subject = statusInfo.TaskName, //status
                Body = ChangeBody(lang.Value, statusInfo),
                From = lang.Value == "tr" ? new MailAddress("diskbackup@narbulut.com", "Narbulut Bilgilendirme") : new MailAddress("diskbackup@narbulut.com", "Narbulut Information")
            })

                try
                {
                    foreach (var item in emailAddresses)
                    {
                        message.To.Add(item.EmailAddress);
                    }

                    smtp.Send(message);
                    _logger.Information("E-Mail gönderimi başarılı.");
                }
                catch (SmtpException ex)
                {
                    _logger.Error(ex, "E-Mail gönderimi başarısız.");
                }
        }

        public void SendEMail(StatusInfo statusInfo)
        {
            var emailList = _emailInfoDal.GetList();

            var emailActive = _configurationDataDal.Get(x => x.Key == "emailActive");
            var emailSuccessful = _configurationDataDal.Get(x => x.Key == "emailSuccessful");
            var emailFail = _configurationDataDal.Get(x => x.Key == "emailFail");
            var emailCritical = _configurationDataDal.Get(x => x.Key == "emailCritical");

            if (emailActive.Value == "True")
            {
                if (statusInfo.Status == StatusType.Success && emailSuccessful.Value == "True")
                {
                    EMailSender(emailList, statusInfo);
                }
                else if (statusInfo.Status == StatusType.Fail && emailFail.Value == "True")
                {
                    EMailSender(emailList, statusInfo);
                }
                else if ((statusInfo.Status == StatusType.ConnectionError || statusInfo.Status == StatusType.NotEnoughDiskSpace || statusInfo.Status == StatusType.MissingFile) && emailCritical.Value == "True")
                {
                    EMailSender(emailList, statusInfo);
                }
            }
        }

        private ConfigurationData GetLang()
        {
            var lang = _configurationDataDal.Get(x => x.Key == "lang");
            if (lang == null)
                lang = new ConfigurationData { Key = "lang", Value = "tr" };

            return lang;
        }

        private string ChangeBody(string lang, StatusInfo statusInfo)
        {
            string body = string.Empty;

            body = ChangeLang(lang);

            body = body.Replace("{FileName}", statusInfo.FileName);
            body = body.Replace("{Duration}", statusInfo.TimeElapsed.ToString()); //hesaplama
            body = body.Replace("{SourceInfo}", statusInfo.SourceObje);
            body = body.Replace("{TaskName}", statusInfo.TaskName);
            body = body.Replace("{AverageDataTransfer}", Math.Round(statusInfo.AverageDataRate, 2).ToString() + " MB/s");
            body = body.Replace("{ProcessedData}", statusInfo.DataProcessed.ToString()); //HESAPLAMA
            body = body.Replace("{InstantDataTransfer}", Math.Round(statusInfo.InstantDataRate, 2).ToString() + " MB/s");
            body = body.Replace("{txtWelcome}", DateTime.Now.ToString() + " Tarihli blabla"); // biz yazıcaz

            return body;
        }

        private string ChangeLang(string lang)
        {
            /*string body = string.Empty;

            try
            {
                using (StreamReader reader = new StreamReader(@"HTML\EMailTemplate.html"))
                {
                    body = reader.ReadToEnd();
                    _logger.Information("4-2-1");
                }
            }
            catch (Exception ex)
            {
                _logger.Information("E-Mail hata: " + ex);
            }*/


            string body = GetHTMLBackupBody();

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

        private string GetHTMLBackupBody()
        {
            return @"<!DOCTYPE html>
                    <html>
                    <head>
                        <style>
                            .tableData {
                                text-align: left; 
                                padding: 8px; 
                                border: 1px solid #f2f2f2;
                            }
                        </style>
                    </head>
                    <body>
                        <div style=""padding: 0px 10% 0px 10%;"">
                        <p style=""text-align: center;"">
                            <span>
                                <a href=""http://panel.narbulut.com"" target=""_blank"" rel=""noopener noreferrer"" data-auth=""NotApplicable"">
                                    <img src=""https://panel.narbulut.com/img/slider/Logoü.png"" alt=""Örnek Resim"" />
                                </a>
                            </span>
                        </p>
                        <h2>{Hello},</h2>
                        <div style=""padding: 0px; border: 0px solid #ffff; margin: 0px 0px 30px 0px;"">
                            <p style=""font-family: Trebuchet MS, sans-serif, serif, EmojiFont;""> 
                                {txtWelcome}
                            </p>
                            <p style=""font-family: Trebuchet MS, sans-serif, serif, EmojiFont;"">
                                {ListTextLang}
                            </p>
                        </div>
                        <table style=""font-family: Trebuchet MS, sans-serif, serif, EmojiFont; border-collapse: collapse; width: 100%;"">
                            <tr style=""background-color:#4CAF50; border:1pt solid #DDDDDD; color: white;"">
                                <th colspan=""2"" style=""padding: 8px; text-align:center; border: 1px solid #f2f2f2;"">{StatusInfoLang}</th>
                            </tr>
                            <tr>
                                <td class=""tableData"">{TaskNameLang}</td>
                                <td class=""tableData"">{TaskName}</td>
                            </tr>
                            <tr>
                                <td class=""tableData"">{FileNameLang}</td>
                                <td class=""tableData"">{FileName}</td>
                            </tr>
                            <tr>
                                <td class=""tableData"">{DurationLang}</td>
                                <td class=""tableData"">{Duration}</td>
                            </tr>
                            <tr>
                                <td class=""tableData"">{AverageDataTransferLang}</td>
                                <td class=""tableData"">{AverageDataTransfer}</td>
                            </tr>
                            <tr>
                                <td class=""tableData"">{ProcessedDataLang}</td>
                                <td class=""tableData"">{ProcessedData}</td>
                            </tr>
                            <tr>
                                <td class=""tableData"">{InstantDataTransferLang}</td>
                                <td class=""tableData"">{InstantDataTransfer}</td>
                            </tr>
                            <tr>
                                <td class=""tableData"">{SourceInfoLang}</td>
                                <td class=""tableData"">{SourceInfo}</td>
                            </tr>
                        </table>
                        <p style=""font-family: Trebuchet MS, sans-serif, serif, EmojiFont;"">
                            {RespectLang}, Narbulut
                        </p>
                            <div style=""padding:18.75pt 0;"">
                                <p align=""center"" style=""text-align:center;margin-top:0;line-height:18.0pt;"">
                                    <span style=""color:#74787E;font-size:9pt;"">
                                        <a href=""http://panel.narbulut.com"" target=""_blank"" rel=""noopener noreferrer"" data-auth=""NotApplicable"">
                                            <span style=""color:#3869D4;"">Copyright Narbulut © 2017</span>
                                        </a>| {AllRightReservedLang}
                                    </span>
                                </p>
                            </div>
                        </div >
                    </body >
                    </html >";
        }
    }
}

