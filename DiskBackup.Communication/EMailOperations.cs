﻿using DiskBackup.DataAccess.Abstract;
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
                else if ((taskInfo.StatusInfo.Status == StatusType.ConnectionError || taskInfo.StatusInfo.Status == StatusType.NotEnoughDiskSpace || taskInfo.StatusInfo.Status == StatusType.MissingFile))
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
                else if ((taskInfo.StatusInfo.Status == StatusType.ConnectionError || taskInfo.StatusInfo.Status == StatusType.NotEnoughDiskSpace || taskInfo.StatusInfo.Status == StatusType.MissingFile))
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
            body = body.Replace("{Duration}", FormatMilliseconds(TimeSpan.FromMilliseconds(taskInfo.StatusInfo.TimeElapsed), lang)); // ---
            body = body.Replace("{SourceInfo}", taskInfo.StatusInfo.SourceObje);
            body = body.Replace("{TaskName}", taskInfo.StatusInfo.TaskName);
            body = body.Replace("{AverageDataTransfer}", Math.Round(taskInfo.StatusInfo.AverageDataRate, 2).ToString() + " MB/s");
            body = body.Replace("{ProcessedData}", FormatBytes(taskInfo.StatusInfo.DataProcessed)); //---
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

            var customerName = _configurationDataDal.Get(x => x.Key == "customerName");
            if (customerName == null)
                customerName = new ConfigurationData { Value = "Demo" };
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
            string body = GetHTMLBackupBody();

            var customerName = _configurationDataDal.Get(x => x.Key == "customerName");
            if (customerName == null)
                customerName = new ConfigurationData { Value = "Demo" };
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
                            #statusInfo {
                                font-family: Trebuchet MS, sans-serif, serif, EmojiFont;
                                border-collapse: collapse;
                                width: 100%;
                            }
                            #statusInfo td, #statusInfo th {
                                text-align: left;
                                padding: 8px;
                                border: 1px solid #f2f2f2;
                            }
                            #statusInfo th {
                                background-color: {BackgroundStatus};
                                color: white;
                                text-align: center;
                            }
                            .leftColumn {
                                font-weight: bold;
                                width: 25%;
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
                        <h2>{Dear}, {customerName}</h2>
                        <div style=""padding: 0px; margin: 0px 0px 30px 0px;"">
                            <p style=""font-family: Trebuchet MS, sans-serif, serif, EmojiFont;""> 
                                {txtWelcome} {ListTextLang}
                            </p>
                        </div>
                        <table id=""statusInfo"">
                            <thead>
                                <tr class=""colorTable"" >
                                    <th colspan=""2"">{StatusInfoLang}</th>
                                </tr>
                            </thead>
                            <tbody>
                                <tr>
                                    <td class=""leftColumn"">{TaskNameLang}</td>
                                    <td style=""width: 75%"">{TaskName}</td>
                                </tr>
                                <tr>
                                    <td class=""leftColumn"">{FileNameLang}</td>
                                    <td>{FileName}</td>
                                </tr>
                                <tr>
                                    <td class=""leftColumn"">{DurationLang}</td>
                                    <td>{Duration}</td>
                                </tr>
                                <tr>
                                    <td class=""leftColumn"">{AverageDataTransferLang}</td>
                                    <td>{AverageDataTransfer}</td>
                                </tr>
                                <tr>
                                    <td class=""leftColumn"">{ProcessedDataLang}</td>
                                    <td>{ProcessedData}</td>
                                </tr>
                                <tr>
                                    <td class=""leftColumn"">{InstantDataTransferLang}</td>
                                    <td>{InstantDataTransfer}</td>
                                </tr>
                                <tr>
                                    <td class=""leftColumn"">{SourceInfoLang}</td>
                                    <td>{SourceInfo}</td>
                                </tr>
                            </tbody>
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
                            <h2>{Dear}, {customerName}</h2>
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


        /*ChangLang içinde
         * string body = string.Empty;

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
    }
}

