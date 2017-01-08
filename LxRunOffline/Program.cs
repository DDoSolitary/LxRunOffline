using System.Diagnostics;
using EasyHook;

namespace LxRunOffline
{
	class Program
	{
		static void Main(string[] args)
		{
			RemoteHooking.CreateAndInject(@"C:\Windows\System32\LxRun.exe", "/install", 0, "LxRunHook.dll", "LxRunHook.dll", out var pId);
			Process.GetProcessById(pId).WaitForExit();
		}
	}
}
