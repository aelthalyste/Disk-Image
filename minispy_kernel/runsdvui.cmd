cd /d "C:\Disk-Image\minispy_kernel" &&msbuild "minispy.vcxproj" /t:sdv /p:inputs="/check /devenv" /p:configuration="Release" /p:platform="x64" /p:SolutionDir="C:\Disk-Image" 
exit 0 