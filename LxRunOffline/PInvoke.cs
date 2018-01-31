using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace LxRunOffline {
	static class PInvoke {
		[StructLayout(LayoutKind.Sequential)]
		public class LxssEaData {
			public short Reserved1;
			public short Version = 1;
			public int Mode;
			public int Uid;
			public int Gid;
			public int Reserved2;
			public int AtimeNsec;
			public int MtimeNsec;
			public int CtimeNsec;
			public long Atime;
			public long Mtime;
			public long Ctime;
		}

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
		public static extern bool EnumerateDirectory(
			SafeFileHandle hFile,
			[MarshalAs(UnmanagedType.LPWStr)]out string fileName,
			out bool directory
		);

		[DllImport("LxssEa.dll", CallingConvention = CallingConvention.Cdecl)]
		public static extern SafeFileHandle GetFileHandle(
			[MarshalAs(UnmanagedType.LPWStr)]string ntPath,
			bool directory,
			bool create,
			bool write
		);

		[DllImport("LxssEa.dll", CallingConvention = CallingConvention.Cdecl)]
		public static extern bool CopyLxssEa(SafeFileHandle hFrom, SafeFileHandle hTo);

		[DllImport("LxssEa.dll", CallingConvention = CallingConvention.Cdecl)]
		public static extern bool SetLxssEa(SafeFileHandle hFile, LxssEaData data, int dataLength);
	}
}
