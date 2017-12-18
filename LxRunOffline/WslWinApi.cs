using System;
using System.Runtime.InteropServices;

namespace LxRunOffline {
	class WslWinApi {
		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		public static extern uint WslLaunchInteractive(
			string distributionName,
			string command,
			bool useCurrentWorkingDirectory,
			out uint exitCode
		);

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		public static extern uint WslRegisterDistribution(string distributionName, string tarGzFilename);
	}
}