using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace DriveInformationOnDotNetFrameworkPoC
{
    class DriveInformation
    {
        public static void Main()
        {
            //DriveInfo[] allDrives = DriveInfo.GetDrives();
            
            //foreach (DriveInfo d in allDrives)
            //{
            //    Console.WriteLine("Drive {0}", d.Name);
            //    Console.WriteLine("  Drive type: {0}", d.DriveType);
            //    if (d.IsReady == true)
            //    {
            //        Console.WriteLine("  Volume label: {0}", d.VolumeLabel);
            //        Console.WriteLine("  File system: {0}", d.DriveFormat);
            //        Console.WriteLine(
            //            "  Available space to current user:{0, 15} bytes",
            //            d.AvailableFreeSpace);

            //        Console.WriteLine(
            //            "  Total available space:          {0, 15} bytes",
            //            d.TotalFreeSpace);

            //        Console.WriteLine(
            //            "  Total size of drive:            {0, 15} bytes ",
            //            d.TotalSize);
            //        Console.WriteLine("------------------------------------");

            //        Console.WriteLine(
            //            "  Available space to current user:{0, 15} bytes",
            //            FormatBytes(d.AvailableFreeSpace));

            //        Console.WriteLine(
            //            "  Total available space:          {0, 15} bytes",
            //            FormatBytes(d.TotalFreeSpace));

            //        Console.WriteLine(
            //            "  Total size of drive:            {0, 15} bytes ",
            //            FormatBytes(d.TotalSize));
            //        Console.WriteLine("------------------------------------> " + d.);
            //        //MBR ya da gpt olan diski yedek aldığımızda 
            //        //Nas 
            //    }
            //    //d.TotalSize - d.TotalFreeSpace = Kullanılan alan
            //}
            //Console.ReadLine();
        }

        public class Volume
        {
            public Volume(string path)
            {
                Path = path;
                ulong freeBytesAvail, totalBytes, totalFreeBytes;
                if (GetDiskFreeSpaceEx(path, out freeBytesAvail, out totalBytes, out totalFreeBytes))
                {
                    FreeBytesAvailable = freeBytesAvail;
                    TotalNumberOfBytes = totalBytes;
                    TotalNumberOfFreeBytes = totalFreeBytes;
                }
            }

            public string Path { get; private set; }

            public ulong FreeBytesAvailable { get; private set; }
            public ulong TotalNumberOfBytes { get; private set; }
            public ulong TotalNumberOfFreeBytes { get; private set; }

            [DllImport("kernel32.dll", SetLastError = true)]
            static extern bool GetDiskFreeSpaceEx([MarshalAs(UnmanagedType.LPStr)] string volumeName, out ulong freeBytesAvail,
                out ulong totalBytes, out ulong totalFreeBytes);
        }

        public class VolumeEnumerator : IEnumerable<Volume>
        {
            public IEnumerator<Volume> GetEnumerator()
            {
                StringBuilder sb = new StringBuilder(2048);
                IntPtr volumeHandle = FindFirstVolume(sb, (uint)sb.MaxCapacity);
                {
                    if (volumeHandle == IntPtr.Zero)
                        yield break;
                    else
                    {
                        do
                        {
                            yield return new Volume(sb.ToString());
                            sb.Clear();
                        }
                        while (FindNextVolume(volumeHandle, sb, (uint)sb.MaxCapacity));
                        FindVolumeClose(volumeHandle);
                    }
                }

            }

            System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
            {
                return GetEnumerator();
            }

            [DllImport("kernel32.dll", SetLastError = true)]
            static extern IntPtr FindFirstVolume([Out] StringBuilder lpszVolumeName,
               uint cchBufferLength);

            [DllImport("kernel32.dll", SetLastError = true)]
            static extern bool FindNextVolume(IntPtr hFindVolume, [Out] StringBuilder lpszVolumeName, uint cchBufferLength);

            [DllImport("kernel32.dll", SetLastError = true)]
            static extern bool FindVolumeClose(IntPtr hFindVolume);
        }

        public static string FormatBytes(long bytes)
        {
            string[] Suffix = { "B", "KB", "MB", "GB", "TB" };
            int i;
            double dblSByte = bytes;
            for (i = 0; i < Suffix.Length && bytes >= 1024; i++, bytes /= 1024)
            {
                dblSByte = bytes / 1024.0;
            }

            if (i > 4)
                i = 4;

            return $"{dblSByte:0.##} {Suffix[i]}";
        }
    }
}
