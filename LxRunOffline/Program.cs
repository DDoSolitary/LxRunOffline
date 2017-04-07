using System;
using System.IO;
using System.Diagnostics;
using EasyHook;

namespace LxRunOffline
{
	class Program
	{
		static void Write(object s)
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.Write(s);
			Console.ResetColor();
		}

		static void WriteLine(object s) { Write(s + Environment.NewLine); }

		static void Main(string[] args)
		{
			try
			{
				Write("Enter path to the Ubuntu image file: ");
				var imagePath = Path.GetFullPath(Console.ReadLine());
				Write("Enter path to the icon file: ");
				var iconPath = Path.GetFullPath(Console.ReadLine());
				RemoteHooking.CreateAndInject(@"C:\Windows\System32\LxRun.exe", string.Join(" ", args), 0, "LxRunHook.dll", "LxRunHook.dll", out var pId, imagePath, iconPath);
				var process = Process.GetProcessById(pId);
				process.WaitForExit();
			}
			catch (Exception e)
			{
				WriteLine("Error: Failed to launch LxRun.");
				WriteLine(e);
				Environment.Exit(-1);
			}
		}
	}
}
