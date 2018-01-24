using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace LxRunOffline {
	static class PInvoke {
		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		public static extern uint WslLaunchInteractive(
			string distributionName,
			string command,
			bool useCurrentWorkingDirectory,
			out uint exitCode
		);

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		public static extern uint WslRegisterDistribution(string distributionName, string tarGzFilename);

		[DllImport("LxssEa.dll", CallingConvention = CallingConvention.Cdecl)]
		public static extern SafeFileHandle GetFileHandle(
			[MarshalAs(UnmanagedType.LPWStr)]string path,
			bool directory,
			bool create,
			bool write
		);

		[DllImport("LxssEa.dll", CallingConvention = CallingConvention.Cdecl)]
		public static extern bool CopyLxssEa(
			SafeFileHandle hFrom,
			SafeFileHandle hTo
		);
	}
}
