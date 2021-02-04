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

        private void EMailSender(List<EmailInfo> emailAddresses, TaskInfo taskInfo)
        {
            ConfigurationData lang = GetLang();
            if (lang == null)
                lang = new ConfigurationData { Key = "lang", Value = "tr" };

            using (var message = new MailMessage("diskbackup@narbulut.com", "diskbackup@narbulut.com")
            {
                IsBodyHtml = true,
                Subject = taskInfo.Name, //status + uniq key
                Body = ChangeBody(lang.Value, taskInfo),
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

        public void SendEMail(TaskInfo taskInfo)
        {
            var emailList = _emailInfoDal.GetList();

            var emailActive = _configurationDataDal.Get(x => x.Key == "emailActive");
            var emailSuccessful = _configurationDataDal.Get(x => x.Key == "emailSuccessful");
            var emailFail = _configurationDataDal.Get(x => x.Key == "emailFail");
            var emailCritical = _configurationDataDal.Get(x => x.Key == "emailCritical");

            if (emailActive.Value == "True")
            {
                if (taskInfo.StatusInfo.Status == StatusType.Success && emailSuccessful.Value == "True")
                {
                    EMailSender(emailList, taskInfo);
                }
                else if (taskInfo.StatusInfo.Status == StatusType.Fail && emailFail.Value == "True")
                {
                    EMailSender(emailList, taskInfo);
                }
                else if ((taskInfo.StatusInfo.Status == StatusType.ConnectionError || taskInfo.StatusInfo.Status == StatusType.NotEnoughDiskSpace || taskInfo.StatusInfo.Status == StatusType.MissingFile) && emailCritical.Value == "True")
                {
                    EMailSender(emailList, taskInfo);
                }
            }
        }

        public void SendTestEMail()
        {
            var emailList = _emailInfoDal.GetList();
            ConfigurationData lang = GetLang();
            if (lang == null)
                lang = new ConfigurationData { Key = "lang", Value = "tr" };

            using (var message = new MailMessage("diskbackup@narbulut.com", "diskbackup@narbulut.com")
            {
                IsBodyHtml = true,
                Subject = lang.Value == "tr" ? "Narbulut Bilgilendirme - Test Email" : "Narbulut Information - Test Email",
                Body = ChangeTestBody(lang.Value),
                From = lang.Value == "tr" ? new MailAddress("diskbackup@narbulut.com", "Narbulut Bilgilendirme") : new MailAddress("diskbackup@narbulut.com", "Narbulut Information")
            })

                try
                {
                    foreach (var item in emailList)
                    {
                        message.To.Add(item.EmailAddress);
                    }

                    smtp.Send(message);
                    _logger.Information("Test e-mail gönderimi başarılı.");
                }
                catch (SmtpException ex)
                {
                    _logger.Error(ex, "Test e-mail gönderimi başarısız.");
                }
        }

        private ConfigurationData GetLang()
        {
            var lang = _configurationDataDal.Get(x => x.Key == "lang");
            if (lang == null)
                lang = new ConfigurationData { Key = "lang", Value = "tr" };

            return lang;
        }

        private string ChangeBody(string lang, TaskInfo taskInfo)
        {
            string body = string.Empty;

            body = ChangeLang(lang, taskInfo);

            body = body.Replace("{FileName}", taskInfo.StatusInfo.FileName);
            body = body.Replace("{Duration}", taskInfo.StatusInfo.TimeElapsed.ToString()); //hesaplama
            body = body.Replace("{SourceInfo}", taskInfo.StatusInfo.SourceObje);
            body = body.Replace("{TaskName}", taskInfo.StatusInfo.TaskName);
            body = body.Replace("{AverageDataTransfer}", Math.Round(taskInfo.StatusInfo.AverageDataRate, 2).ToString() + " MB/s");
            body = body.Replace("{ProcessedData}", taskInfo.StatusInfo.DataProcessed.ToString()); //HESAPLAMA
            body = body.Replace("{InstantDataTransfer}", Math.Round(taskInfo.StatusInfo.InstantDataRate, 2).ToString() + " MB/s");

            if (taskInfo.StatusInfo.Status == StatusType.Success)
                body = body.Replace("{BackgroundStatus}", "green");
            else if (taskInfo.StatusInfo.Status == StatusType.Fail)
                body = body.Replace("{BackgroundStatus}", "red");
            else if (taskInfo.StatusInfo.Status == StatusType.ConnectionError || taskInfo.StatusInfo.Status == StatusType.MissingFile || taskInfo.StatusInfo.Status == StatusType.NotEnoughDiskSpace)
                body = body.Replace("{BackgroundStatus}", "orange");

            return body;
        }

        private string ChangeTestBody(string lang)
        {
            string body = string.Empty;

            body = GetHTMLTestBody();

            if (lang == "en")
            {
                #region En 
                body = body.Replace("{Hello}", "Hello");
                body = body.Replace("{txtWelcomeTest}", "This is a test email.");
                body = body.Replace("{RespectLang}", "Best Regards");
                body = body.Replace("{AllRightReservedLang}", "All Right Reserved");
                #endregion
            }
            else
            {
                #region TR
                body = body.Replace("{Hello}", "Merhaba");
                body = body.Replace("{txtWelcomeTest}", "Bu bir test emailidir.");
                body = body.Replace("{RespectLang}", "Saygılarımızla");
                body = body.Replace("{AllRightReservedLang}", "Tüm Hakları Saklıdır");
                #endregion
            }

            return body;
        }

        private string ChangeLang(string lang, TaskInfo taskInfo)
        {
            /*string body = string.Empty;

            try
            {
                using (StreamReader reader = new StreamReader(@"HTML\EMailTemplate.html"))
                {
                    body = reader.ReadToEnd();
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
                body = body.Replace("{ListTextLang}", "The list below shows details of the task;");
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

                if (taskInfo.StatusInfo.Status == StatusType.Success)
                    body = body.Replace("{txtWelcome}", taskInfo.StatusInfo.TaskName + " task dated " + taskInfo.LastWorkingDate.ToString() + " was successful.");
                else if (taskInfo.StatusInfo.Status == StatusType.Fail)
                    body = body.Replace("{txtWelcome}", taskInfo.StatusInfo.TaskName + " task dated " + taskInfo.LastWorkingDate.ToString() + " failed.");
                else if (taskInfo.StatusInfo.Status == StatusType.ConnectionError)
                    body = body.Replace("{txtWelcome}", taskInfo.StatusInfo.TaskName + " task dated " + taskInfo.LastWorkingDate.ToString() + " failed due to connection error.");
                else if (taskInfo.StatusInfo.Status == StatusType.MissingFile)
                    body = body.Replace("{txtWelcome}", taskInfo.StatusInfo.TaskName + " task dated " + taskInfo.LastWorkingDate.ToString() + " failed due to missing file error.");
                else if (taskInfo.StatusInfo.Status == StatusType.NotEnoughDiskSpace)
                    body = body.Replace("{txtWelcome}", taskInfo.StatusInfo.TaskName + " task dated " + taskInfo.LastWorkingDate.ToString() + " failed due to not enough disk space error.");
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

                if (taskInfo.StatusInfo.Status == StatusType.Success)
                    body = body.Replace("{txtWelcome}", taskInfo.LastWorkingDate.ToString() + " tarihli " + taskInfo.StatusInfo.TaskName + " görevi başarılı olmuştur.");
                else if (taskInfo.StatusInfo.Status == StatusType.Fail)
                    body = body.Replace("{txtWelcome}", taskInfo.LastWorkingDate.ToString() + " tarihli " + taskInfo.StatusInfo.TaskName + " görevi başarısız olmuştur.");
                else if (taskInfo.StatusInfo.Status == StatusType.ConnectionError)
                    body = body.Replace("{txtWelcome}", taskInfo.LastWorkingDate.ToString() + " tarihli " + taskInfo.StatusInfo.TaskName + " görevi bağlantı hatasından dolayı başarısız olmuştur.");
                else if (taskInfo.StatusInfo.Status == StatusType.MissingFile)
                    body = body.Replace("{txtWelcome}", taskInfo.LastWorkingDate.ToString() + " tarihli " + taskInfo.StatusInfo.TaskName + " görevi eksik dosya hatasından dolayı başarısız olmuştur.");
                else if (taskInfo.StatusInfo.Status == StatusType.NotEnoughDiskSpace)
                    body = body.Replace("{txtWelcome}", taskInfo.LastWorkingDate.ToString() + " tarihli " + taskInfo.StatusInfo.TaskName + " görevi yetersiz disk alanı hatasından dolayı başarısız olmuştur.");
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
                            <tr style=""background-color:{BackgroundStatus}; border:1pt solid #DDDDDD; color: white;"">
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

        private string GetHTMLTestBody()
        {
            return @"<!DOCTYPE html>
                    <head>
                        <meta charset=""utf-8"" />
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
                                    {txtWelcomeTest}
                                </p>
                            </div>
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
                        </div>
                    </body>
                    </html>";
        }
    }
}

