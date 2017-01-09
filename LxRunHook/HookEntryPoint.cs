using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using EasyHook;

namespace LxRunHook
{
	public class HookEntryPoint : IEntryPoint
	{
		const string urlTemplate = "https://go.microsoft.com/fwlink/?LinkID=";

		string imagePath, iconPath;
		Dictionary<IntPtr, FileStream> handleDic = new Dictionary<IntPtr, FileStream>();

		public HookEntryPoint(RemoteHooking.IContext context) { }

		void Write(object s)
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.Write(s);
			Console.ResetColor();
		}

		void WriteLine(object s) { Write(s + Environment.NewLine); }

		[DllImport("wininet.dll", SetLastError = true)]
		static extern IntPtr InternetOpenA(string lpszAgent, int dwAccessType, string lpszProxyName, string lpszProxyBypass, int dwFlags);

		#region InternetOpenUrlA

		[UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true)]
		delegate IntPtr InternetOpenUrlADelegate(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext);

		[DllImport("wininet.dll", SetLastError = true)]
		static extern IntPtr InternetOpenUrlA(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext);

		IntPtr InternetOpenUrlAHook(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext)
		{
			IntPtr hUrl = IntPtr.Zero;
			if (string.Equals(lpszUrl, urlTemplate + "747853", StringComparison.OrdinalIgnoreCase) ||
				string.Equals(lpszUrl, urlTemplate + "730581", StringComparison.OrdinalIgnoreCase) ||
				string.Equals(lpszUrl, urlTemplate + "827586", StringComparison.OrdinalIgnoreCase))
			{
				try
				{
					// Get a dummy handle
					hUrl = InternetOpenA("", 0, null, null, 0);
					handleDic.Add(hUrl, File.OpenRead(lpszUrl.EndsWith("747853") ? iconPath : imagePath));
				}
				catch (Exception e)
				{
					WriteLine("Error: Failed to open the file.");
					WriteLine(e);
					return IntPtr.Zero;
				}
			}
			else hUrl = InternetOpenUrlA(hInternet, lpszUrl, lpszHeaders, dwHeadersLength, dwContext);
			return hUrl;
		}

		#endregion

		#region InternetCloseHandle

		[UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true)]
		delegate bool InternetCloseHandleDelegate(IntPtr hInternet);

		[DllImport("wininet.dll", SetLastError = true)]
		static extern bool InternetCloseHandle(IntPtr hInternet);

		bool InternetCloseHandleHook(IntPtr hInternet)
		{
			try
			{
				if (handleDic.ContainsKey(hInternet))
				{
					var file = handleDic[hInternet];
					handleDic.Remove(hInternet);
					file.Dispose();
				}
			}
			catch (Exception e)
			{
				WriteLine("Warning: Failed to close the file.");
				WriteLine(e);
				return false;
			}
			return InternetCloseHandle(hInternet);
		}

		#endregion

		#region InternetReadFile

		[UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true)]
		delegate bool InternetReadFileDelegate(IntPtr hFile, IntPtr lpBuffer, int dwNumberOfBytesToRead, out int lpdwNumberOfBytesRead);

		[DllImport("wininet.dll", SetLastError = true)]
		static extern bool InternetReadFile(IntPtr hFile, IntPtr lpBuffer, int dwNumberOfBytesToRead, out int lpdwNumberOfBytesRead);

		bool InternetReadFileHook(IntPtr hFile, IntPtr lpBuffer, int dwNumberOfBytesToRead, out int lpdwNumberOfBytesRead)
		{
			if (!handleDic.ContainsKey(hFile)) return InternetReadFile(hFile, lpBuffer, dwNumberOfBytesToRead, out lpdwNumberOfBytesRead);
			FileStream file = handleDic[hFile];
			try
			{
				var buffer = new byte[dwNumberOfBytesToRead];
				lpdwNumberOfBytesRead = file.Read(buffer, 0, dwNumberOfBytesToRead);
				Marshal.Copy(buffer, 0, lpBuffer, lpdwNumberOfBytesRead);
				return true;
			}
			catch (Exception e)
			{
				WriteLine("Error: Failed to read the file.");
				WriteLine(e);
				lpdwNumberOfBytesRead = 0;
				return false;
			}
		}

		#endregion

		public void Run(RemoteHooking.IContext context)
		{
			Write("Enter path to the Ubuntu image file: ");
			imagePath = Console.ReadLine();
			Write("Enter path to the icon file: ");
			iconPath = Console.ReadLine();
			try
			{
				using (var hook1 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetOpenUrlA"), new InternetOpenUrlADelegate(InternetOpenUrlAHook), null))
				using (var hook2 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetCloseHandle"), new InternetCloseHandleDelegate(InternetCloseHandleHook), null))
				using (var hook3 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetReadFile"), new InternetReadFileDelegate(InternetReadFileHook), null))
				{
					hook1.ThreadACL.SetExclusiveACL(new[] { 0 });
					hook2.ThreadACL.SetExclusiveACL(new[] { 0 });
					hook3.ThreadACL.SetExclusiveACL(new[] { 0 });
					RemoteHooking.WakeUpProcess();
					Thread.Sleep(Timeout.Infinite);
				}
			}
			catch (Exception e)
			{
				WriteLine("Error: Failed to install hooks in LxRun.");
				WriteLine(e);
				Environment.Exit(-1);
			}
		}
	}
}
