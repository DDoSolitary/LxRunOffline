using System;
using System.Runtime.InteropServices;

namespace LxRunOffline {
	partial class Program {
		[Flags]
		enum DistroFlags : uint {
			None = 0,
			EnableInterop = 1,
			AppendNtPath = 2,
			EnableDriveMounting = 4
		}

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		static extern uint WslConfigureDistribution(
			string distributionName,
			uint defaultUID,
			DistroFlags wslDistributionFlags
		);

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		static extern uint WslGetDistributionConfiguration(
			string distributionName,
			out uint distributionVersion,
			out uint defaultUID,
			out DistroFlags flags,
			out IntPtr envVars,
			out uint envVarCount
		);

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		static extern bool WslIsDistributionRegistered(string distributionName);

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		static extern uint WslLaunch(
			string distributionName,
			string command,
			bool useCurrentWorkingDirectory,
			IntPtr stdIn,
			IntPtr stdOut,
			IntPtr stdErr,
			out IntPtr processHandle
		);

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		static extern uint WslLaunchInteractive(
			string distributionName,
			string command,
			bool useCurrentWorkingDirectory,
			out uint errorCode
		);

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		static extern uint WslRegisterDistribution(string distributionName, string tarGzFilename);

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		static extern uint WslUnregisterDistribution(string distributionName);
	}
}