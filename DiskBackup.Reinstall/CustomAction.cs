using Microsoft.Deployment.WindowsInstaller;
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Windows.Forms;

namespace DiskBackup.Reinstall
{
    public class CustomActions
    {
        [CustomAction]
        public static ActionResult VerifyReinstall(Session session)
        {
            string quartzDbPath = @"C:\ProgramData\NarDiskBackup\disk_image_quartz.db", imageDbPath = @"C:\ProgramData\NarDiskBackup\image_disk.db";
            session.Log("Begin VerifyReinstall");

            if (!File.Exists(@"C:\Program Files\NarDiskBackup\DiskBackupWPFGUI.exe"))
            {
                if (File.Exists(quartzDbPath) && File.Exists(imageDbPath))
                {
                    var response = MessageBox.Show("Eski kurulumdan devam etmek ister misiniz?", "Narbulut Bilgilendirme", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                    if (response == DialogResult.Yes)
                    {
                        //yeniden yüklemek istemedi
                        return ActionResult.Success;
                    }
                    else
                    {
                        var response2 = MessageBox.Show("Bu iþlemden sonra yapýlan deðiþiklikler geri alýnamaz, onaylýyor musunuz?", "Narbulut Bilgilendirme", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                        if (response2 == DialogResult.Yes)
                        {
                            try
                            {
                                File.Delete(quartzDbPath);
                                File.Delete(imageDbPath);
                                return ActionResult.Success;
                            }
                            catch (Exception ex)
                            {
                                session.Log("Dosyalar silinemedi. " + ex.Message);
                                return ActionResult.Failure;
                            }
                        }
                        else
                        {
                            session.Log("Kullanýcý iptal etti.");
                            return ActionResult.UserExit;
                        }
                    }
                }
                else if (File.Exists(quartzDbPath))
                {
                    try
                    {
                        File.Delete(quartzDbPath);
                        return ActionResult.Success;
                    }
                    catch (Exception ex)
                    {
                        session.Log("QuartzDb silinemedi. " + ex.Message);
                        return ActionResult.Failure;
                    }
                }
                else if (File.Exists(imageDbPath))
                {
                    try
                    {
                        File.Delete(imageDbPath);
                        return ActionResult.Success;
                    }
                    catch (Exception ex)
                    {
                        session.Log("ImageDiskDb silinemedi. " + ex.Message);
                        return ActionResult.Failure;
                    }
                }
            }
                
            return ActionResult.Success;
        }
    }
}
