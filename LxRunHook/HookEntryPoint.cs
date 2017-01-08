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
		string imagePath, iconPath;
		IntPtr imageHandle, iconHandle;
		FileStream imageFile, iconFile;

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
			var hUrl = InternetOpenUrlA(hInternet, lpszUrl, lpszHeaders, dwHeadersLength, dwContext);
			if (hUrl != IntPtr.Zero)
			{
				try
				{
					if (lpszUrl.EndsWith("747853"))
					{
						iconFile = File.OpenRead(iconPath);
						iconHandle = hUrl;
					}
					else if (lpszUrl.EndsWith("730581") || lpszUrl.EndsWith("827586"))
					{
						imageFile = File.OpenRead(imagePath);
						imageHandle = hUrl;
					}
				}
				catch (Exception e)
				{
					WriteLine(e);
					return IntPtr.Zero;
				}
			}
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
			if (hInternet == imageHandle)
			{
				imageFile.Dispose();
				imageHandle = IntPtr.Zero;
			}
			else if (hInternet == iconHandle)
			{
				iconFile.Dispose();
				iconHandle = IntPtr.Zero;
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
			try
			{
				FileStream file = null;
				if (hFile == imageHandle) file = imageFile;
				else if (hFile == iconHandle) file = iconFile;
				if (file == null || hFile == IntPtr.Zero) return InternetReadFile(hFile, lpBuffer, dwNumberOfBytesToRead, out lpdwNumberOfBytesRead);
				var buffer = new byte[dwNumberOfBytesToRead];
				lpdwNumberOfBytesRead = file.Read(buffer, 0, dwNumberOfBytesToRead);
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
				imagePath = Console.ReadLine();
				Write("Enter path to the icon file: ");
				iconPath = Console.ReadLine();
				using (var hook1 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetOpenUrlA"), new InternetOpenUrlADelegate(InternetOpenUrlAHook), null))
				using (var hook2 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetCloseHandle"), new InternetCloseHandleDelegate(InternetCloseHandleHook), null))
				using (var hook3 = LocalHook.Create(LocalHook.GetProcAddress("wininet.dll", "InternetReadFile"), new InternetReadFileDelegate(InternetReadFileHook), null))
				{
					hook1.ThreadACL.SetExclusiveACL(new[] { 0 });
					hook2.ThreadACL.SetExclusiveACL(new[] { 0 });
					hook3.ThreadACL.SetExclusiveACL(new[] { 0 });
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
