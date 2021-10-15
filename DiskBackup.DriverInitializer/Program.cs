using System;
using System.Diagnostics;

namespace DiskBackup.DriverInitializer
{
    class Program
    {
        static void Main(string[] args)
        {
            ProcessStartInfo myProcessInfo = new ProcessStartInfo(); //Initializes a new ProcessStartInfo of name myProcessInfo
            myProcessInfo.FileName = Environment.ExpandEnvironmentVariables("%SystemRoot%") + @"\System32\cmd.exe"; //Sets the FileName property of myProcessInfo to %SystemRoot%\System32\cmd.exe where %SystemRoot% is a system variable which is expanded using Environment.ExpandEnvironmentVariables
            myProcessInfo.Arguments = @"/C cd\ && cd ""Program Files\NarDiskBackup"" && rundll32 syssetup,SetupInfObjectInstallAction DefaultInstall 128 .\minispy.inf && net start minispy && exit";
            myProcessInfo.Verb = "runas"; //The process should start with elevated permissions
            Process.Start(myProcessInfo); //Starts the process based on myProcessInfo
        }
    }
}
