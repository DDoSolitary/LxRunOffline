using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using Microsoft.Win32;

namespace LxRunOffline
{
	partial class Program
	{		
		static void Error(string output) {
			Console.Error.WriteLine(output);
			Environment.Exit(1);
		}
		
		static void CheckResult(uint errorCode) {
			if (errorCode != 0) Error($"Error: {errorCode.ToString("X").PadLeft(8, '0')}");
		}
		
		static void CheckExists(string distroName) {
			if (!WslIsDistributionRegistered(distroName)) {
				Error("Distro name not found");
			}
		}

		static int Main(string[] args)
		{
			if (args.Length == 0) Error("Invalid arguments.");

			var action = args[0].ToLower();
			if (action == "run") {

				string cmd = "";
				if (args.Length == 2) {
					cmd = "/bin/bash --login";
				} else if (args.Length == 3) {
					cmd = args[2];
				} else Error("Invalid arguments.");
				CheckExists(args[1]);

				CheckResult(WslLaunchInteractive(args[1], cmd, true, out var errorCode));
				return (int)errorCode;

			} else if (action == "install") {

				if (args.Length != 4) Error("Invalid arguments.");

				if (WslIsDistributionRegistered(args[1])) Error("Distro name already exists.");
				var tarGzPath = Path.GetFullPath(args[2]);

				if (!File.Exists(tarGzPath)) Error("Install file not found.");
				var targetDirectory = Path.GetFullPath(args[3]).TrimEnd('\\');
				if (!Directory.Exists(targetDirectory)) Error("Target directory not found.");
				if (Directory.Exists(Path.Combine(targetDirectory, "rootfs")))
					Error("The \"rootfs\" directory already exists under the target directory.");

				var exeDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
				if (Directory.Exists(Path.Combine(exeDirectory, "rootfs")))
					Error("The \"rootfs\" directory already exists under the directory containing this program.");

				CheckResult(WslRegisterDistribution(args[1], tarGzPath));

				if (!string.Equals(exeDirectory, targetDirectory, StringComparison.InvariantCultureIgnoreCase)) {
					Directory.Move(Path.Combine(exeDirectory, "rootfs"), Path.Combine(targetDirectory, "rootfs"));
					using (var lxssKey = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Lxss")) {
						foreach (var keyName in lxssKey.GetSubKeyNames()) {
							using (var distroKey = lxssKey.OpenSubKey(keyName, true)) {
								if ((string)distroKey.GetValue("DistributionName") == args[1]) {
									distroKey.SetValue("BasePath", targetDirectory);
									break;
								}
							}
						}
					}
				}

			} else if (action == "uninstall") {

				if (args.Length != 2) Error("Invalid arguments.");
				CheckExists(args[1]);

				CheckResult(WslUnregisterDistribution(args[1]));

			} else if (action == "info") {

				if (args.Length != 2) Error("Invalid arguments.");
				CheckExists(args[1]);

				CheckResult(WslGetDistributionConfiguration(
					args[1],
					out var version,
					out var uid,
					out var flags,
					out var envVars,
					out var envCount
				));

				Console.WriteLine("Version: {0}", version);
				Console.WriteLine("Default UID: {0}", uid);
				Console.WriteLine("Windows interop: {0}", flags.HasFlag(DistroFlags.EnableInterop));
				Console.WriteLine("Append PATH from Windows: {0}", flags.HasFlag(DistroFlags.AppendNtPath));
				Console.WriteLine("Mount Windows drives: {0}", flags.HasFlag(DistroFlags.EnableDriveMounting));

				Console.WriteLine();
				Console.WriteLine("Default environment variables:");
				for (var i = 0; i < envCount; i++) {
					var pStr = Marshal.ReadIntPtr(envVars + i*IntPtr.Size);
					Console.WriteLine(Marshal.PtrToStringAnsi(pStr));
					Marshal.FreeHGlobal(pStr);
				}
				Marshal.FreeHGlobal(envVars);
				Console.WriteLine();

			} else if (action == "config") {

				if (args.Length != 6) Error("Invalid arguments");
				CheckExists(args[1]);

				if (!uint.TryParse(args[2], out var uid)) Error("Invalid arguments.");

				DistroFlags flags = DistroFlags.None;
				int parsed;
				if (int.TryParse(args[3], out parsed)) {
					if (parsed != 0) flags |= DistroFlags.EnableInterop;
				} else Error("Invalid arguments.");
				if (int.TryParse(args[4], out parsed)) {
					if (parsed != 0) flags |= DistroFlags.AppendNtPath;
				} else Error("Invalid arguments.");
				if (int.TryParse(args[5], out parsed)) {
					if (parsed != 0) flags |= DistroFlags.EnableDriveMounting;
				} else Error("Invalid arguments.");

				CheckResult(WslConfigureDistribution(args[1], uid, flags));

			} else if (action == "list") {

				if (args.Length != 1) Error("Invalid arguments.");

				using (var lxssKey = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Lxss")) {
					foreach (var keyName in lxssKey.GetSubKeyNames()) {
						using (var distroKey = lxssKey.OpenSubKey(keyName)) {
							Console.WriteLine(distroKey.GetValue("DistributionName"));
						}
					}
				}
				Console.WriteLine();

			} else Error("Invalid arguments.");

			Console.WriteLine("Done.");
			return 0;
		}
	}
}
