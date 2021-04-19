using DiskBackup.Business.Abstract;
using DiskBackup.DataAccess.Abstract;
using DiskBackup.Entities.Concrete;
using Microsoft.Win32;
using Serilog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace DiskBackup.Business.Concrete
{
    public class LicenseService : ILicenseService
    {
        private readonly IConfigurationDataDal _configurationDataDal;
        private readonly ILogger _logger;
        private string RegistryPath = "SOFTWARE\\NarDiskBackup"; //Registry.LocalMachine.OpenSubKey
        private string FilePath = "C:\\Windows\\system32\\eyruebup.dll";

        /* ExpireDate -> 
         * License -> 
         * LastDate -> 
         * UploadDate -> 
         * Type -> 
         */

        public LicenseService(IConfigurationDataDal configurationDataDal, ILogger logger)
        {
            _configurationDataDal = configurationDataDal;
            _logger = logger;
        }

        public string GetRegistryType()
        {
            var key = Registry.LocalMachine.OpenSubKey(RegistryPath);
            return key.GetValue("Type").ToString();
        }

        public int GetDemoDaysLeft()
        {
            var key = Registry.LocalMachine.OpenSubKey(RegistryPath);
            return (Convert.ToDateTime(key.GetValue("ExpireDate").ToString()) - DateTime.Now).Days;
        }

        public string[] GetLicenseUserInfo()
        {
            var key = Registry.LocalMachine.OpenSubKey(RegistryPath);
            var licenseKey = (string)key.GetValue("License");
            var resultDecryptLicenseKey = DecryptLicenseKey(licenseKey);
            return resultDecryptLicenseKey.Split('_');
        }

        public string GetLicenseKey()
        {
            var key = Registry.LocalMachine.OpenSubKey(RegistryPath);
            var licenseKey = (string)key.GetValue("License");
            return DecryptLicenseKey(licenseKey);
        }

        public void UpdateDemoLastDate()
        {
            var key = Registry.LocalMachine.OpenSubKey(RegistryPath, true);
            key.SetValue("LastDate", DateTime.Now);
        }

        public bool IsDemoExpired()
        {
            StreamReader streamReader = new StreamReader(FilePath);
            var resultString = streamReader.ReadToEnd().Split('_');
            streamReader.Close();

            var key = Registry.LocalMachine.OpenSubKey(RegistryPath);
            return Convert.ToDateTime(key.GetValue("UploadDate").ToString()) <= DateTime.Now &&
                Convert.ToDateTime(key.GetValue("ExpireDate").ToString()) >= DateTime.Now &&
                Convert.ToDateTime(key.GetValue("LastDate").ToString()) <= DateTime.Now &&
                (Convert.ToDateTime(key.GetValue("ExpireDate").ToString()) - Convert.ToDateTime(key.GetValue("UploadDate").ToString())).Days < 31 &&
                Convert.ToDateTime(key.GetValue("ExpireDate")) == Convert.ToDateTime(resultString[3]) &&
                Convert.ToDateTime(key.GetValue("UploadDate")) == Convert.ToDateTime(resultString[1]);
        }

        public void FixBrokenRegistry()
        {
            var key = CreateRegistryFile();
            key.SetValue("UploadDate", DateTime.Now);
            key.SetValue("ExpireDate", DateTime.Now - TimeSpan.FromDays(1));
            key.SetValue("Type", 1505);
        }

        public void DeleteRegistryFile()
        {
            try
            {
                Registry.LocalMachine.DeleteSubKey(RegistryPath);
            }
            catch(Exception)
            { }
        }

        public void SetDemoFile(string customerName)
        {
            FileStream fileStream = new FileStream(FilePath, FileMode.OpenOrCreate, FileAccess.Write);
            StreamWriter streamWriter = new StreamWriter(fileStream);
            DateTime dateTimeNow = DateTime.Now;
            streamWriter.WriteLine("UploadDate_" + dateTimeNow + "_ExpireDate_" + (dateTimeNow + TimeSpan.FromDays(30)));
            streamWriter.Flush();
            streamWriter.Close();
            fileStream.Close();
            SetRegistryDemo(customerName, dateTimeNow);
        }

        private void SetRegistryDemo(string customerName, DateTime dateTimeNow)
        {
            //_logger.Information("Demo lisans aktifleştirildi.");
            RegistryKey key = CreateRegistryFile();
            key.SetValue("UploadDate", dateTimeNow);
            key.SetValue("ExpireDate", dateTimeNow + TimeSpan.FromDays(30));
            key.SetValue("LastDate", dateTimeNow);
            key.SetValue("Type", 1505);
            var customerConfiguration = _configurationDataDal.Get(x => x.Key == "customerName");
            if (customerConfiguration == null)
            {
                customerConfiguration = new ConfigurationData { Key = "customerName", Value = customerName };
                _configurationDataDal.Add(customerConfiguration);
            }
            else
            {
                customerConfiguration.Value = customerName;
                _configurationDataDal.Update(customerConfiguration);
            }
        }

        private void SetRegistryLicense(string licenseKey)
        {
            RegistryKey key = CreateRegistryFile();
            key.SetValue("UploadDate", DateTime.Now);
            key.SetValue("ExpireDate", "");
            key.SetValue("Type", 2606);
            key.SetValue("License", licenseKey);
        }

        private RegistryKey CreateRegistryFile()
        {
            var key = Registry.LocalMachine.OpenSubKey(RegistryPath, true);

            if (key == null) // null ise dosyayı oluştur
            {
                //_logger.Information("Lisans dosyası oluşturuldu.");
                key = Registry.LocalMachine.CreateSubKey(RegistryPath);
            }

            return key;
        }

        public bool ThereIsARegistryFile()
        {
            var key = Registry.LocalMachine.OpenSubKey(RegistryPath);
            if (key == null)
                return false; // dosya oluşturulmamış

            return true; // dosya var
        }

        public bool ThereIsAFile()
        {
            return File.Exists(FilePath);
        }

        public string ValidateLicenseKey(string licenseKey)
        {
            var resultDecryptLicenseKey = DecryptLicenseKey(licenseKey);
            if (resultDecryptLicenseKey.Equals("fail"))
            {
                _logger.Error("Lisans anahtarı doğru girilmedi. Lisans Anahtarı: " + licenseKey);
                return "fail";
            }
            else if (resultDecryptLicenseKey.Equals("failMachine"))
            {
                _logger.Error("Lisans anahtarı makine türünüz uyumsuz. Lisans Anahtarı: " + licenseKey);
                return "failMachine";
            }
            else if (resultDecryptLicenseKey.Equals("failOS"))
            {
                _logger.Error("Lisans anahtarı versiyonları uyumsuz. Lisans Anahtarı: " + licenseKey);
                return "failOS";
            }
            else
            {
                _logger.Information("Lisans aktifleştirildi. Lisans Anahtarı: " + licenseKey);
                SetRegistryLicense(licenseKey);
                AddDBCustomerNameAndUniqKey(resultDecryptLicenseKey);
                return resultDecryptLicenseKey;
            }
        }

        public void AddDBCustomerNameAndUniqKey(string DecryptLicenseKey)
        {
            try
            {
                var splitLicenseKey = DecryptLicenseKey.Split('_');
                var customerConfiguration = _configurationDataDal.Get(x => x.Key == "customerName");
                if (customerConfiguration == null)
                {
                    customerConfiguration = new ConfigurationData { Key = "customerName", Value = splitLicenseKey[1] };
                    _configurationDataDal.Add(customerConfiguration);
                }
                else if (customerConfiguration.Value != splitLicenseKey[1])
                {
                    customerConfiguration.Value = splitLicenseKey[1];
                    _configurationDataDal.Update(customerConfiguration);
                }
                var uniqKeyConfiguration = _configurationDataDal.Get(x => x.Key == "uniqKey");
                if (uniqKeyConfiguration == null)
                {
                    uniqKeyConfiguration = new ConfigurationData { Key = "uniqKey", Value = splitLicenseKey[7] };
                    _configurationDataDal.Add(uniqKeyConfiguration);
                }
                else if (uniqKeyConfiguration.Value != splitLicenseKey[7])
                {
                    uniqKeyConfiguration.Value = splitLicenseKey[7];
                    _configurationDataDal.Update(uniqKeyConfiguration);
                }
            }
            catch (Exception ex)
            {
                _logger.Error("Müşteri isimi ve uniq key veritabanına eklenemiyor. Error: " + ex);
            }
        }

        private string DecryptLicenseKey(string cipherLicenseKey)
        {
            try
            {
                var iv = Convert.FromBase64String("EEXkANPr+5R9q+XyG7jR5w==");
                byte[] buffer = Convert.FromBase64String(cipherLicenseKey);

                using (Aes aes = Aes.Create())
                {
                    aes.Key = Encoding.UTF8.GetBytes("D*G-KaPdSgVkYp3s6v8y/B?E(H+MbQeT");
                    aes.IV = iv;
                    ICryptoTransform decryptor = aes.CreateDecryptor(aes.Key, aes.IV);

                    using (MemoryStream memoryStream = new MemoryStream(buffer))
                    {
                        using (CryptoStream cryptoStream = new CryptoStream((Stream)memoryStream, decryptor, CryptoStreamMode.Read))
                        {
                            using (StreamReader streamReader = new StreamReader((Stream)cryptoStream))
                            {
                                var decryptLicenseKey = streamReader.ReadToEnd();
                                if (!CheckMachine(decryptLicenseKey.Split('_')[5]))
                                    return "failMachine";
                                if (CheckOSVersion(decryptLicenseKey.Split('_')[6]))
                                    return decryptLicenseKey;
                                else
                                    return "failOS";
                            }
                        }
                    }
                }
            }
            catch (Exception)
            {
                return "fail";
            }
        }

        private bool CheckOSVersion(string versionType)
        {
            RegistryKey registryKey = Registry.LocalMachine.OpenSubKey("Software\\Microsoft\\Windows NT\\CurrentVersion");
            var OSName = (string)registryKey.GetValue("ProductName");

            if (versionType.Equals("SBS") && OSName.Contains("Small Business Server"))
            {
                return true;
            }
            else if (versionType.Equals("Server") && OSName.Contains("Server"))
            {
                return true;
            }
            else if (versionType.Equals("Workstation") && OSName.Contains("Windows"))
            {
                return true;
            }

            return false;
        }

        private bool CheckMachine(string machineType)
        {
            RegistryKey registryKey = Registry.LocalMachine.OpenSubKey("HARDWARE\\DESCRIPTION\\System\\BIOS");
            var pathName = registryKey.GetValueNames();

            if (machineType.Equals("VirtualMachine"))
            {
                foreach (var item in pathName)
                {
                    if (registryKey.GetValue(item).ToString().Contains("VMware") || registryKey.GetValue(item).ToString().Contains("Hyper-V") || registryKey.GetValue(item).ToString().Contains("Virtual"))
                        return true;
                }
            }
            else
            {
                foreach (var item in pathName)
                {
                    if (registryKey.GetValue(item).ToString().Contains("VMware") || registryKey.GetValue(item).ToString().Contains("Hyper-V") || registryKey.GetValue(item).ToString().Contains("Virtual"))
                        return false;
                }
                return true;
            }

            return false;
        }

    }
}
