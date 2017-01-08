using System;
using System.Diagnostics;
using EasyHook;

namespace LxRunOffline
{
	class Program
	{
		static void Main(string[] args)
		{
			int pId = 0;
			try
			{
				RemoteHooking.CreateAndInject(@"C:\Windows\System32\LxRun.exe", "/install", 0, "LxRunHook.dll", "LxRunHook.dll", out pId);
			}
			catch (Exception e)
			{
				Console.ForegroundColor = ConsoleColor.Yellow;
				Console.WriteLine("Error: Failed to launch LxRun.");
				Console.WriteLine(e);
				Environment.Exit(-1);
			}
			var process = Process.GetProcessById(pId);
			process.WaitForExit();
		}
	}
}
