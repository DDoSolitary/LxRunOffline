using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using Microsoft.Win32;

namespace LxRunOffline {
	enum DistroFlags : int {
		None = 0,
		EnableInterop = 1,
		AppendNtPath = 2,
		EnableDriveMounting = 4
	}

	class Wsl {

		const string LxssKeyPath = @"Software\Microsoft\Windows\CurrentVersion\Lxss";
		const string RobocopyArguments = "/E /COPYALL /NFL /NDL /IS /IT";
		const int DeletionRetryCount = 3;

		#region Helpers

		static void Error(string output) {
			Console.Error.WriteLine(output);
			Environment.Exit(1);
		}

		static void CheckWinApiResult(uint errorCode) {
			if (errorCode != 0) Error($"Error: {errorCode.ToString("X").PadLeft(8, '0')}");
		}

		static RegistryKey GetLxssKey(bool write = false) {
			return Registry.CurrentUser.CreateSubKey(LxssKeyPath, write);
		}

		static RegistryKey FindDistroKey(string distroName, bool write = false) {
			using (var lxssKey = GetLxssKey()) {
				foreach (var keyName in lxssKey.GetSubKeyNames()) {
					using (var distroKey = lxssKey.OpenSubKey(keyName)) {
						if ((string)distroKey.GetValue("DistributionName") == distroName) {
							return lxssKey.OpenSubKey(keyName, write);
						}
					}
				}
			}
			return null;
		}

		static object GetRegistryValue(string distroName, string valueName) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) Error("Name not found.");
				return distroKey.GetValue(valueName);
			}
		}

		static void SetRegistryValue(string distroName, string valueName, object value) {
			using (var distroKey = FindDistroKey(distroName, true)) {
				if (distroKey == null) Error("Name not found.");
				distroKey.SetValue(valueName, value);
			}
		}

		static void DeleteDirectory(string path) {
			var retryCount = DeletionRetryCount;
			while (true) {
				retryCount--;
				try {
					Directory.Delete(path, true);
					return;
				} catch (IOException e) {
					Console.WriteLine($"Error: {e.Message}");
					if (retryCount == 0) {
						Console.WriteLine($"Couldn't delete the directory \"{path}\", you may have to delete it manually.");
					} else {
						Console.WriteLine($"Couldn't delete the directory \"{path}\", retrying.");
					}
				}
			}
		}

		static void MoveDirectory(string oldPath, string newPath) {
			try {
				Directory.Move(oldPath, newPath);
				return;
			} catch (IOException e) {
				Console.WriteLine($"Error: {e.Message}");
				Console.WriteLine("Couldn't move the directory falling back to use robocopy.");
				// TODO: warning
			}

			var startInfo = new ProcessStartInfo {
				FileName = "robocopy",
				Arguments = $"{RobocopyArguments} \"{oldPath}\" \"{newPath}\"",
				Verb = "runas"
			};
			using (var process = Process.Start(startInfo)) {
				process.WaitForExit();
				if (process.ExitCode > 1) {
					Error($"robocopy exited with a non-successful code: {process.ExitCode}");
				}
			}
			DeleteDirectory(oldPath);
		}

		#endregion

		#region Global operations

		public static IEnumerable<string> ListDistros() {
			using (var lxssKey = GetLxssKey()) {
				foreach (var keyName in lxssKey.GetSubKeyNames()) {
					using (var distroKey = lxssKey.OpenSubKey(keyName)) {
						yield return (string)distroKey.GetValue("DistributionName");
					}
				}
			}
		}

		public static string GetDefaultDistro() {
			using (var lxssKey = GetLxssKey(true)) {
				using (var distroKey = lxssKey.OpenSubKey((string)lxssKey.GetValue("DefaultDistribution") ?? string.Empty)) {
					if (distroKey == null) Error("Distro not found.");
					return (string)distroKey.GetValue("DistributionName");
				}
			}
		}

		public static void SetDefaultDistro(string distroName) {
			string distroKeyName = "";

			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) Error("Name not found.");
				distroKeyName = Path.GetFileName(distroKey.Name);
			}

			using (var lxssKey = GetLxssKey(true)) {
				lxssKey.SetValue("DefaultDistribution", distroKeyName);
			}
		}

		#endregion

		#region Distro operations

		public static void InstallDistro(string distroName, string tarGzPath, string targetPath) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey != null) Error("Name already exists.");
			}
			if (!File.Exists(tarGzPath)) Error("File not found.");
			if (Directory.Exists(targetPath)) Error("Target directory already exists.");

			string tmpRootPath = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "rootfs");
			if (Directory.Exists(tmpRootPath))
				Error("The \"rootfs\" directory already exists in the directory containing the program.");

			CheckWinApiResult(WslWinApi.WslRegisterDistribution(distroName, Path.GetFullPath(tarGzPath)));

			Directory.CreateDirectory(targetPath);
			MoveDirectory(tmpRootPath, Path.Combine(targetPath, "rootfs"));

			SetInstallationDirectory(distroName, targetPath);
		}

		public static void RegisterDistro(string distroName, string installPath) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey != null) Error("Name already exists.");
			}
			if (!Directory.Exists(installPath)) Error("Installation directory not found.");

			using (var lxssKey = GetLxssKey(true))
			using (var distroKey = lxssKey.CreateSubKey(Guid.NewGuid().ToString("B"))) {
				distroKey.SetValue("DistributionName", distroName);
				distroKey.SetValue("BasePath", Path.GetFullPath(installPath).TrimEnd('\\'));
				distroKey.SetValue("State", 1);
				distroKey.SetValue("Version", 1);
			}
		}

		public static void UninstallDistro(string distroName) {
			var installPath = GetInstallationDirectory(distroName);
			if (!Directory.Exists(installPath)) Error("Installation directory not found.");

			UnregisterDistro(distroName);
			DeleteDirectory(installPath);
		}

		public static void UnregisterDistro(string distroName) {
			string distroKeyName = "";

			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) Error("Name not found.");
				distroKeyName = Path.GetFileName(distroKey.Name);
			}

			using (var lxssKey = GetLxssKey(true)) {
				lxssKey.DeleteSubKey(distroKeyName);
			}
		}

		public static void MoveDistro(string distroName, string newPath) {
			if (Directory.Exists(newPath)) Error("Target directory already exists.");

			var oldPath = GetInstallationDirectory(distroName);
			MoveDirectory(oldPath, newPath);
			SetInstallationDirectory(distroName, newPath);
		}

		public static uint LaunchDistro(string distroName, string command) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) Error("Name not found.");
			}

			CheckWinApiResult(WslWinApi.WslLaunchInteractive(distroName, command, true, out var exitCode));
			return exitCode;
		}

		#endregion

		#region Distro config operations

		public static string GetInstallationDirectory(string distroName) {
			return (string)GetRegistryValue(distroName, "BasePath");
		}

		static void SetInstallationDirectory(string distroName, string installPath) {
			SetRegistryValue(distroName, "BasePath", Path.GetFullPath(installPath).TrimEnd('\\'));
		}

		public static string[] GetDefaultEnvironment(string distroName) {
			return (string[])(GetRegistryValue(distroName, "DefaultEnvironment")
				?? new string[] {
					"HOSTTYPE=x86_64",
					"LANG=en_US.UTF-8",
					"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games",
					"TERM=xterm-256color"
				});
		}

		public static void SetDefaultEnvironment(string distroName, string[] environmentVariables) {
			SetRegistryValue(distroName, "DefaultEnvironment", environmentVariables);
		}

		public static int GetDefaultUid(string distroName) {
			return (int)(GetRegistryValue(distroName, "DefaultUid") ?? 0);
		}

		public static void SetDefaultUid(string distroName, int uid) {
			SetRegistryValue(distroName, "DefaultUid", uid);
		}

		public static string GetKernelCommandLine(string distroName) {
			return (string)GetRegistryValue(distroName, "KernelCommandLine") ?? "BOOT_IMAGE=/kernel init=/init ro";
		}

		public static void SetKernelCommandLine(string distroName, string commandLine) {
			SetRegistryValue(distroName, "KernelCommandLine", commandLine);
		}

		public static bool GetFlag(string distroName, DistroFlags mask) {
			return ((DistroFlags)(GetRegistryValue(distroName, "Flags") ?? 7) & mask) > 0;
		}

		public static void SetFlag(string distroName, DistroFlags mask, bool value) {
			var flag = (DistroFlags)(GetRegistryValue(distroName, "Flags") ?? 7);
			SetRegistryValue(distroName, "Flags", (int)(flag & ~mask | (value ? mask : 0)));
		}

		#endregion

	}
}
