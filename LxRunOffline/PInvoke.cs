using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace LxRunOffline {
	static class PInvoke {
		[StructLayout(LayoutKind.Sequential)]
		public class LxssEaData {
			public ushort Reserved1;
			public ushort Version = 1;
			public uint Mode;
			public uint Uid;
			public uint Gid;
			public uint Reserved2;
			public uint AtimeNsec;
			public uint MtimeNsec;
			public uint CtimeNsec;
			public ulong Atime;
			public ulong Mtime;
			public ulong Ctime;
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
		public static extern SafeFileHandle GetFileHandle(
			[MarshalAs(UnmanagedType.LPWStr)]string path,
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
