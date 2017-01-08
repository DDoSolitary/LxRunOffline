using System;
using EasyHook;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Threading;
using System.IO;

namespace LxRunHook
{
	public class HookEntryPoint : IEntryPoint
	{
		FileStream imageFile, iconFile, currentFile;

		public HookEntryPoint(RemoteHooking.IContext context) { }

		void Write(object s)
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.Write(s);
			Console.ResetColor();
		}

		void WriteLine(object s) { Write(s + Environment.NewLine); }


		#region InternetOpenUrlA

		[UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true)]
		delegate IntPtr InternetOpenUrlADelegate(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext);

		[DllImport("wininet.dll", SetLastError = true)]
		static extern IntPtr InternetOpenUrlA(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext);

		IntPtr InternetOpenUrlAHook(IntPtr hInternet, string lpszUrl, string lpszHeaders, int dwHeadersLength, IntPtr dwContext)
		{
			var hFile = InternetOpenUrlA(hInternet, lpszUrl, lpszHeaders, dwHeadersLength, dwContext);
			if (lpszUrl.EndsWith("747853")) currentFile = iconFile;
			else if (lpszUrl.EndsWith("730581") || lpszUrl.EndsWith("827586")) currentFile = imageFile;
			return hFile;
		}

		#endregion

		#region InternetReadFile

		[UnmanagedFunctionPointer(CallingConvention.StdCall, SetLastError = true)]
		delegate bool InternetReadFileDelegate(IntPtr hFile, IntPtr lpBuffer, int dwNumberOfBytesToRead, out int lpdwNumberOfBytesRead);

		[DllImport("wininet.dll", SetLastError = true)]
		static extern bool InternetReadFile(IntPtr hFile, IntPtr lpBuffer, int dwNumberOfBytesToRead, out int lpdwNumberOfBytesRead);

		bool InternetReadFileHook(IntPtr hFile, IntPtr lpBuffer, int dwNumberOfBytesToRead, out int lpdwNumberOfBytesRead)
		{
			try
			{
				if (currentFile == null) return InternetReadFile(hFile, lpBuffer, dwNumberOfBytesToRead, out lpdwNumberOfBytesRead);
				var buffer = new byte[dwNumberOfBytesToRead];
				lpdwNumberOfBytesRead = currentFile.Read(buffer, 0, dwNumberOfBytesToRead);
				if (lpdwNumberOfBytesRead == 0)
				{
					currentFile.Dispose();
					currentFile = null;
				}
				Marshal.Copy(buffer, 0, lpBuffer, lpdwNumberOfBytesRead);
				return true;
			}
			catch (Exception e)
			{
				WriteLine(e);
				Environment.FailFast(e.Message);
				lpdwNumberOfBytesRead = 0;
				return false;
			}
		}

		#endregion

		public void Run(RemoteHooking.IContext context)
		{
			try
			{
				Write("Enter path to the Ubuntu image file: ");
				imageFile = File.OpenRead(Console.ReadLine());
				Write("Enter path to the icon file: ");
				iconFile = File.OpenRead(Console.ReadLine());
				using (var hook1 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetOpenUrlA"), new InternetOpenUrlADelegate(InternetOpenUrlAHook), null))
				using (var hook2 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetReadFile"), new InternetReadFileDelegate(InternetReadFileHook), null))
				{
					hook1.ThreadACL.SetExclusiveACL(new[] { 0 });
					hook2.ThreadACL.SetExclusiveACL(new[] { 0 });
					WriteLine("Hooked: " + Process.GetCurrentProcess().ProcessName);
					RemoteHooking.WakeUpProcess();
					Thread.Sleep(Timeout.Infinite);
				}
			}
			catch (Exception e)
			{
				WriteLine(e);
				Environment.FailFast(e.Message);
			}
		}
	}
}
