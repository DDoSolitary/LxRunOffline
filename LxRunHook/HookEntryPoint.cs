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
		Dictionary<IntPtr, string> handleDic = new Dictionary<IntPtr, string>();

		public HookEntryPoint(RemoteHooking.IContext context, string imagePath, string iconPath) { }

		#region InternetOpenUrlA

		[UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true)]
		delegate IntPtr InternetOpenUrlADelegate(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext);

		[DllImport("wininet.dll", SetLastError = true)]
		static extern IntPtr InternetOpenUrlA(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext);

		IntPtr InternetOpenUrlAHook(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext)
		{
			if (string.Equals(lpszUrl, urlTemplate + "747853", StringComparison.OrdinalIgnoreCase) ||
				string.Equals(lpszUrl, urlTemplate + "730581", StringComparison.OrdinalIgnoreCase) ||
				string.Equals(lpszUrl, urlTemplate + "827586", StringComparison.OrdinalIgnoreCase))
			{
				var path = lpszUrl.EndsWith("747853") ? iconPath : imagePath;
				var hUrl = InternetOpenUrlA(hInternet, "file://" + path, lpszHeaders, dwHeadersLength, dwContext);
				handleDic.Add(hUrl, path);
				return hUrl;
			}
			return InternetOpenUrlA(hInternet, lpszUrl, lpszHeaders, dwHeadersLength, dwContext);
		}

		#endregion

		#region HttpQueryInfoA

		[UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true)]
		delegate bool HttpQueryInfoADelegate(IntPtr hRequest, int dwInfoLevel, IntPtr lpvBuffer, ref uint lpdwBufferLength, IntPtr lpdwIndex);

		[DllImport("wininet.dll", SetLastError = true)]
		static extern bool HttpQueryInfoA(IntPtr hRequest, int dwInfoLevel, IntPtr lpvBuffer, ref uint lpdwBufferLength, IntPtr lpdwIndex);

		bool HttpQueryInfoAHook(IntPtr hRequest, int dwInfoLevel, IntPtr lpvBuffer, ref uint lpdwBufferLength, IntPtr lpdwIndex)
		{
			if (handleDic.ContainsKey(hRequest))
			{
				if (dwInfoLevel != 0x20000000 + 5 || !File.Exists(handleDic[hRequest])) return false;
				var length = lpdwBufferLength;
				lpdwBufferLength = 4;
				if (length < 4) return false;
				Marshal.WriteInt32(lpvBuffer, (int)new FileInfo(handleDic[hRequest]).Length);
				return true;
			}
			return HttpQueryInfoA(hRequest, dwInfoLevel, lpvBuffer, ref lpdwBufferLength, lpdwIndex);
		}

		#endregion

		#region InternetCloseHandle

		[UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true)]
		delegate bool InternetCloseHandleDelegate(IntPtr hInternet);

		[DllImport("wininet.dll", SetLastError = true)]
		static extern bool InternetCloseHandle(IntPtr hInternet);

		bool InternetCloseHandleHook(IntPtr hInternet)
		{
			if (handleDic.ContainsKey(hInternet)) handleDic.Remove(hInternet);
			return InternetCloseHandle(hInternet);
		}

		#endregion

		public void Run(RemoteHooking.IContext context, string imagePath, string iconPath)
		{
			this.imagePath = imagePath;
			this.iconPath = iconPath;
			Console.WriteLine($"{imagePath} {iconPath}");
			try
			{
				using (var hook1 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetOpenUrlA"), new InternetOpenUrlADelegate(InternetOpenUrlAHook), null))
				using (var hook2 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "HttpQueryInfoA"), new HttpQueryInfoADelegate(HttpQueryInfoAHook), null))
				using (var hook3 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetCloseHandle"), new InternetCloseHandleDelegate(InternetCloseHandleHook), null))
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
				Console.ForegroundColor = ConsoleColor.Yellow;
				Console.WriteLine("Error: Failed to install hooks in LxRun.");
				Console.WriteLine(e);
				Console.ResetColor();
				Environment.Exit(-1);
			}
		}
	}
}
