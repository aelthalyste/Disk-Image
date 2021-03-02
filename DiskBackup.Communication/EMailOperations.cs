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

            using (var message = new MailMessage()
            {
                IsBodyHtml = true,
                Body = ChangeBody(lang.Value, taskInfo),
                From = lang.Value == "tr" ? new MailAddress("diskbackup@narbulut.com", "Narbulut Bilgilendirme") : new MailAddress("diskbackup@narbulut.com", "Narbulut Information")
            })

                try
                {
                    message.Bcc.Add("diskbackup@narbulut.com");
                    SetSubjecj(taskInfo, lang, message);
                    foreach (var item in emailAddresses)
                    {
                        message.To.Add(item.EmailAddress);
                    }

                    smtp.Send(message);
                }
                catch (SmtpException ex)
                {
                    _logger.Error(ex, "E-Mail gönderimi başarısız.");
                }
        }

        private void SetSubjecj(TaskInfo taskInfo, ConfigurationData lang, MailMessage message) // Görev adı, Durumu (Uniq Key)
        {
            if (lang.Value == "tr")
            {
                var uniqKey = _configurationDataDal.Get(x => x.Key == "uniqKey");
                if (uniqKey == null)
                    uniqKey = new ConfigurationData { Value = "Demo" };
                if (taskInfo.StatusInfo.Status == StatusType.Success)
                {
                    message.Subject = taskInfo.Name + ", Başarılı (" + uniqKey.Value + ")";
                }
                else if (taskInfo.StatusInfo.Status == StatusType.Fail)
                {
                    message.Subject = taskInfo.Name + ", Başarısız (" + uniqKey.Value + ")";
                }
                else
                {
                    message.Subject = taskInfo.Name + ", Kritik Hata (" + uniqKey.Value + ")";
                }
            }
            else
            {
                var uniqKey = _configurationDataDal.Get(x => x.Key == "uniqKey");
                if (uniqKey == null)
                    uniqKey = new ConfigurationData { Value = "Demo" };
                if (taskInfo.StatusInfo.Status == StatusType.Success)
                {
                    message.Subject = taskInfo.Name + ", Successful (" + uniqKey.Value + ")";
                }
                else if (taskInfo.StatusInfo.Status == StatusType.Fail)
                {
                    message.Subject = taskInfo.Name + ", Fail (" + uniqKey.Value + ")";
                }
                else
                {
                    message.Subject = taskInfo.Name + ", Critical Error (" + uniqKey.Value + ")";
                }
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
                else if ((taskInfo.StatusInfo.Status != StatusType.Success && taskInfo.StatusInfo.Status != StatusType.Fail) && emailCritical.Value == "True")
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

            body = body.Replace("{TaskName}", taskInfo.StatusInfo.TaskName);
            body = body.Replace("{FileName}", taskInfo.StatusInfo.FileName);
            body = body.Replace("{SourceInfo}", taskInfo.StatusInfo.SourceObje);

            if (taskInfo.Type == TaskType.Backup)
            {
                body = body.Replace("{Duration}", FormatMilliseconds(TimeSpan.FromMilliseconds(taskInfo.StatusInfo.TimeElapsed), lang));
                body = body.Replace("{AverageDataTransfer}", Math.Round(taskInfo.StatusInfo.AverageDataRate, 2).ToString() + " MB/s");
                body = body.Replace("{ProcessedData}", FormatBytes(taskInfo.StatusInfo.DataProcessed));
                body = body.Replace("{InstantDataTransfer}", Math.Round(taskInfo.StatusInfo.InstantDataRate, 2).ToString() + " MB/s");
            }
            else // restore
            {
                TimeSpan duration = DateTime.Now - taskInfo.LastWorkingDate;
                body = body.Replace("{Duration}", FormatMilliseconds(duration, lang));
            }

            if (taskInfo.StatusInfo.Status == StatusType.Success)
                body = body.Replace("{BackgroundStatus}", "green");
            else if (taskInfo.StatusInfo.Status == StatusType.Fail)
                body = body.Replace("{BackgroundStatus}", "red");
            else 
                body = body.Replace("{BackgroundStatus}", "orange");

            return body;
        }

        private string ChangeTestBody(string lang)
        {
            string body = string.Empty;

            body = GetHTMLTestBody();

            var customerName = _configurationDataDal.Get(x => x.Key == "customerName");
            body = body.Replace("{customerName}", customerName.Value);

            if (lang == "en")
            {
                #region En 
                body = body.Replace("{Dear}", "Dear");
                body = body.Replace("{txtWelcomeTest}", "This is a test email.");
                body = body.Replace("{RespectLang}", "Best Regards");
                body = body.Replace("{AllRightReservedLang}", "All Right Reserved");
                #endregion
            }
            else
            {
                #region TR
                body = body.Replace("{Dear}", "Sayın");
                body = body.Replace("{txtWelcomeTest}", "Bu bir test emailidir.");
                body = body.Replace("{RespectLang}", "Saygılarımızla");
                body = body.Replace("{AllRightReservedLang}", "Tüm Hakları Saklıdır");
                #endregion
            }

            return body;
        }

        private string ChangeLang(string lang, TaskInfo taskInfo)
        {
            string body = string.Empty;

            if (taskInfo.Type == TaskType.Backup)
                body = GetHTMLBackupBody();
            else
                body = GetHTMLRestoreBody();

            var customerName = _configurationDataDal.Get(x => x.Key == "customerName");
            body = body.Replace("{customerName}", customerName.Value);

            if (lang == "en")
            {
                #region En 
                body = body.Replace("{Dear}", "Dear");
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
                else if (taskInfo.StatusInfo.Status == StatusType.DriverNotInitialized)
                    body = body.Replace("{txtWelcome}", taskInfo.StatusInfo.TaskName + " task dated " + taskInfo.LastWorkingDate.ToString() + " failed because the driver could not be initialized.");
                else if (taskInfo.StatusInfo.Status == StatusType.PathNotFound)
                    body = body.Replace("{txtWelcome}", taskInfo.StatusInfo.TaskName + " task dated " + taskInfo.LastWorkingDate.ToString() + " failed because the path to save the backup could not be found.");
                #endregion
            }
            else
            {
                #region TR
                body = body.Replace("{Dear}", "Sayın");
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
                else if (taskInfo.StatusInfo.Status == StatusType.DriverNotInitialized)
                    body = body.Replace("{txtWelcome}", taskInfo.LastWorkingDate.ToString() + " tarihli " + taskInfo.StatusInfo.TaskName + " görevi driver başlatılamadığından dolayı başarısız olmuştur.");
                else if (taskInfo.StatusInfo.Status == StatusType.PathNotFound)
                    body = body.Replace("{txtWelcome}", taskInfo.LastWorkingDate.ToString() + " tarihli " + taskInfo.StatusInfo.TaskName + " görevi backup'ın kaydedileceği yol bulunamadığından dolayı başarısız olmuştur.");
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
                            *{
                                text-decoration: none;
                            }
                        </style>
                    </head>
                    <body bgcolor=""#F2F4F6"">
                        <div style=""padding: 0px 22% 0px 22%;"">
                        <p style=""text-align: center; padding-top:50px;"">
                            <span>
                                <a href=""http://panel.narbulut.com"" target=""_blank"" rel=""noopener noreferrer"" data-auth=""NotApplicable"">
                                    <img src=""https://panel.narbulut.com/img/slider/Logoü.png"" alt=""Örnek Resim"" />
                                </a>
                            </span>
                        </p>
                        <h2>{Dear}, {customerName}</h2>
                        <div style=""padding: 0px; margin: 0px 0px 30px 0px;"">
                            <p style=""font-family:Calibri,Candara,Segoe,Segoe UI,Optima,Arial,sans-serif; font-size:18px; "" > 
                                {txtWelcome} {ListTextLang}
                            </p>
                        </div>
                        <table style=""background-color:white; border-collapse:collapse; width:95%;"" align=""center"">
                            <tr>
                                <th style=""font-family:Arial, sans-serif, serif, EmojiFont; text-align:left; padding:20px;"">{Dear}, {customerName}</th>
                            </tr>
                            <tr>
                                <td style=""font-family:Arial, sans-serif, serif, EmojiFont; color:slategray; text-align:left; padding:0px 0px 10px 20px;"">{txtWelcome} {ListTextLang}</td>
                            </tr>
                            <tr>
                                <td style=""padding:20px;"">
                                    <table style=""border-collapse:collapse; width:100%; font-family:Arial, sans-serif, serif, EmojiFont;"">
                                        <thead>
                                            <tr>
                                                <th style=""background-color:#4CAF50; color:white; text-align:center; padding:8px;"" colspan=""3"">{StatusInfoLang}</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:44%;"">{TaskNameLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{TaskName}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{FileNameLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{FileName}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{DurationLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{Duration}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{AverageDataTransferLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{AverageDataTransfer}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{ProcessedDataLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{ProcessedData}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{InstantDataTransferLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{InstantDataTransfer}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{SourceInfoLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{SourceInfo}</td>
                                            </tr>
                                        </tbody>
                                    </table>
                                </td>
                            </tr>
                            <tr>
                                <td style=""font-family:Arial, sans-serif, serif, EmojiFont; color:slategray; padding:10px 0px 20px 20px;"">{RespectLang}, Narbulut</td>
                            </tr>
                        </table>
                        <p style=""font-family:Calibri,Candara,Segoe,Segoe UI,Optima,Arial,sans-serif; font-size:18px;"">
                            {RespectLang}, Narbulut
                        </p>
                            <div style=""padding:18.75pt 0;"">
                                <p align=""center"" style=""text-align:center; margin-top:0; line-height:18.0pt;"">
                                    <span style=""color:#74787E; font-size:9pt;"">
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

        private string GetHTMLTestBody()
        {
            return @"<!DOCTYPE html>
                    <html>
                    <head>
                        <style>
                            *{
                                text-decoration: none;
                            }
                        </style>
                    </head>
                    <body bgcolor=""#F2F4F6"">
                        <div style=""padding: 0px 22% 0px 22%;"">
                        <p style=""text-align: center; padding-top:50px;"">
                            <span>
                                <a href=""http://panel.narbulut.com"" target=""_blank"" rel=""noopener noreferrer"" data-auth=""NotApplicable"">
                                    <img src=""https://panel.narbulut.com/img/slider/Logoü.png"" alt=""Örnek Resim"" />
                                </a>
                            </span>
                        </p>
                            <h2>{Dear}, {customerName}</h2>
                            <div style=""padding: 0px; border: 0px solid #ffff; margin: 0px 0px 30px 0px;"">
                                <p style=""font-family:Calibri,Candara,Segoe,Segoe UI,Optima,Arial,sans-serif; font-size:18px;"">
                                    {txtWelcomeTest}
                                </p>
                            </div>
                            <p style=""font-family:Calibri,Candara,Segoe,Segoe UI,Optima,Arial,sans-serif; font-size:18px;"">
                                {RespectLang}, Narbulut
                            </p>
                            <div style=""padding:18.75pt 0;"">
                                <p align=""center"" style=""text-align:center; margin-top:0; line-height:18.0pt;"">
                                    <span style=""color:#74787E; font-size:9pt;"">
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

        private string GetHTMLRestoreBody()
        {
            return @"<!DOCTYPE html>
                    <html>
                    <head>
                        <meta http-equiv=""Content-Type"" content=""text/html; charset=utf-8"">
                        <style>
                            * {
                                text-decoration: none;
                            }
                        </style>
                    </head>
                    <body bgcolor=""#F2F4F6"">
                        <div style=""padding: 0px 22% 0px 22%;"">
                        <p style=""text-align: center; padding-top:50px;"">
                            <span>
                                <a href=""http://panel.narbulut.com"" target=""_blank"" rel=""noopener noreferrer"" data-auth=""NotApplicable"">
                                    <img src=""https://panel.narbulut.com/img/slider/Logoü.png"" alt=""Örnek Resim"" />
                                </a>
                            </span>
                        </p>
                            <table style=""background-color:white; border-collapse:collapse; width:95%;"" align=""center"">
                            <tr>
                                <th style=""font-family:Arial, sans-serif, serif, EmojiFont; text-align:left; padding:20px;"">{Dear}, {customerName}</th>
                            </tr>
                            <tr>
                                <td style=""font-family:Arial, sans-serif, serif, EmojiFont; color:slategray; text-align:left; padding:0px 0px 10px 20px;"">{txtWelcome} {ListTextLang}</td>
                            </tr>
                            <tr>
                                <td style=""padding:20px;"">
                                    <table style=""border-collapse:collapse; width:100%; font-family:Calibri,Candara,Segoe,Segoe UI,Optima,Arial,sans-serif; font-size:18px;"">
                                        <thead>
                                            <tr>
                                                <th style=""background-color:{BackgroundStatus}; color:white; text-align:center; padding:8px; border:solid 1px #f2f2f2;"" colspan=""2"">{StatusInfoLang}</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:44%;"">{TaskNameLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{TaskName}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{FileNameLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{FileName}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{DurationLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{Duration}</td>
                                            </tr>
                                            <tr>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:34%;"">{SourceInfoLang}</td>
                                                <td style=""text-align:left; padding:8px; font-weight:bold; width:1%;"">:</td>
                                                <td style=""text-align:left; padding:8px; width:65%;"">{SourceInfo}</td>
                                            </tr>
                                        </tbody>
                                    </table>
                                </td>
                            </tr>
                            <tr>
                                <td style=""font-family:Arial, sans-serif, serif, EmojiFont; color:slategray; padding:10px 0px 20px 20px;"">{RespectLang}, Narbulut</td>
                            </tr>
                        </table>
                    <div style=""padding:18.75pt 0;"">
                        <p align=""center"" style=""text-align:center; margin-top:0; line-height:18.0pt;"">
                            <span style=""color:#74787E;font-size:9pt;"">
                                <a href=""http://panel.narbulut.com"" target=""_blank"" rel=""noopener noreferrer"" data-auth=""NotApplicable"">
                                    <span style=""color:#3869D4;"">Copyright Narbulut © 2017</span>
                                </a>| {AllRightReservedLang}
                            </span>
                        </p>
                    </div>
                </div>
            </body>
            </html> ";
        }

        #region dönüştürücü methodlar
        public string FormatMilliseconds(TimeSpan obj, string lang)
        {
            StringBuilder sb = new StringBuilder();
            if (obj.Hours != 0)
            {
                sb.Append(obj.Hours);
                sb.Append(" ");
                if (lang == "en")
                    sb.Append("h");
                else
                    sb.Append("s");
                sb.Append(" ");
            }
            if (obj.Minutes != 0 || sb.Length != 0)
            {
                sb.Append(obj.Minutes);
                sb.Append(" ");
                if (lang == "en")
                    sb.Append("min");
                else
                    sb.Append("dk");
                sb.Append(" ");
            }
            if (obj.Seconds != 0 || sb.Length != 0)
            {
                sb.Append(obj.Seconds);
                sb.Append(" ");
                if (lang == "en")
                    sb.Append("sec");
                else
                    sb.Append("sn");
                sb.Append(" ");
            }
            if (obj.Milliseconds != 0 || sb.Length != 0)
            {
                sb.Append(obj.Milliseconds);
                sb.Append(" ");
                sb.Append("ms");
                sb.Append(" ");
            }
            if (sb.Length == 0)
            {
                sb.Append(0);
                sb.Append(" ");
                sb.Append("ms");
            }
            return sb.ToString();
        }

        private static string FormatBytes(long bytes)
        {
            string[] Suffix = { "B", "KB", "MB", "GB", "TB" };
            int i;
            double dblSByte = bytes;
            for (i = 0; i < Suffix.Length && bytes >= 1024; i++, bytes /= 1024)
            {
                dblSByte = bytes / 1024.0;
            }

            return ($"{dblSByte:0.##} {Suffix[i]}");
        }
        #endregion

    }
}

