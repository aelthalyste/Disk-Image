set "params=%*"
cd /d "%~dp0" && ( if exist "%temp%\getadmin.vbs" del "%temp%\getadmin.vbs" ) && fsutil dirty query %systemdrive% 1>nul 2>nul || ( echo Set UAC = CreateObject^("Shell.Application"^) : UAC.ShellExecute "cmd.exe", "/k cd ""%~sdp0"" && %~s0 %params%", "", "runas", 1 >> "%temp%\getadmin.vbs" && "%temp%\getadmin.vbs" && exit /B )
net stop NarDiskBackupService
TASKKILL/F /IM DiskBackup.Service.exe
del C:\Windows\NARBOOTFILE
del C:\ProgramData\NarDiskBackup\disk_image_quartz.db
del C:\ProgramData\NarDiskBackup\image_disk.db
net stop minispy
del C:\Windows\NAR_LOG_FILE_*
net start minispy