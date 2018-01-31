using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using ICSharpCode.SharpZipLib.GZip;
using Microsoft.Win32;

namespace LxRunOffline {
	enum DistroFlags : int {
		None = 0,
		EnableInterop = 1,
		AppendNtPath = 2,
		EnableDriveMounting = 4
	}

	static class Wsl {

		const string LxssKeyPath = @"Software\Microsoft\Windows\CurrentVersion\Lxss";

		#region P/Invoke

		[DllImport("wslapi.dll", CharSet = CharSet.Unicode)]
		public static extern uint WslLaunchInteractive(
			string distributionName,
			string command,
			bool useCurrentWorkingDirectory,
			out uint exitCode
		);

		#endregion

		#region Helpers

		static void CheckWinApiResult(uint errorCode) {
			if (errorCode != 0) {
				Utils.Error($"Win32 API returned an error: 0x{errorCode.ToString("X").PadLeft(8, '0')}.");
			}
		}

		static void ErrorNameNotFound(string distroName) {
			Utils.Error($"Distribution name not found: {distroName}.");
		}

		static void ErrorNameExists(string distroName) {
			Utils.Error($"Distribution name already exists: {distroName}.");
		}

		static void ErrorDirectoryExists(string distroName) {
			Utils.Error($"Target directory already exists: {distroName}.");
		}

		static void LogDistributionFound(string path) {
			Utils.Log($"Distribution found, ID is {path}.");
		}

		static RegistryKey GetLxssKey(bool write = false) {
			Utils.Log($"Opening the LXSS registry key.");
			return Registry.CurrentUser.CreateSubKey(LxssKeyPath, write);
		}

		static RegistryKey FindDistroKey(string distroName, bool write = false) {
			Utils.Log($"Looking for the registry key of the distribution \"{distroName}\". Write access: {write}.");
			using (var lxssKey = GetLxssKey()) {
				foreach (var keyName in lxssKey.GetSubKeyNames()) {
					using (var distroKey = lxssKey.OpenSubKey(keyName)) {
						if ((string)distroKey.GetValue("DistributionName") == distroName) {
							LogDistributionFound(keyName);
							return lxssKey.OpenSubKey(keyName, write);
						}
					}
				}
			}
			Utils.Log($"Distribution \"{distroName}\" not found.");
			return null;
		}

		static object GetRegistryValue(string distroName, string valueName) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) ErrorNameNotFound(distroName);
				var value = distroKey.GetValue(valueName);
				Utils.Log($"Getting the value of \"{valueName}\", which is \"{value}\".");
				return value;
			}
		}

		static void SetRegistryValue(string distroName, string valueName, object value) {
			using (var distroKey = FindDistroKey(distroName, true)) {
				if (distroKey == null) ErrorNameNotFound(distroName);
				Utils.Log($"Setting the value of \"{valueName}\" to \"{value}\".");
				distroKey.SetValue(valueName, value);
			}
		}

		#endregion

		#region Global operations

		public static IEnumerable<string> ListDistros() {
			using (var lxssKey = GetLxssKey()) {
				foreach (var keyName in lxssKey.GetSubKeyNames()) {
					using (var distroKey = lxssKey.OpenSubKey(keyName)) {
						LogDistributionFound(keyName);
						yield return (string)distroKey.GetValue("DistributionName");
					}
				}
			}
		}

		public static string GetDefaultDistro() {
			using (var lxssKey = GetLxssKey(true)) {
				var keyName = (string)lxssKey.GetValue("DefaultDistribution") ?? string.Empty;
				using (var distroKey = lxssKey.OpenSubKey(keyName)) {
					if (distroKey == null) Utils.Error($"Distribution not found, some registry keys may be corrupt. Set a new default to fix it.");
					LogDistributionFound(keyName);
					return (string)distroKey.GetValue("DistributionName");
				}
			}
		}

		public static void SetDefaultDistro(string distroName) {
			string distroKeyName = "";

			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) ErrorNameNotFound(distroName);
				distroKeyName = Path.GetFileName(distroKey.Name);
			}

			using (var lxssKey = GetLxssKey(true)) {
				Utils.Log($"Setting the value of \"DefaultDistribution\" to \"{distroKeyName}\".");
				lxssKey.SetValue("DefaultDistribution", distroKeyName);
			}
		}

		#endregion

		#region Distro operations

		public static void InstallDistro(string distroName, string tarPath, string targetPath) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey != null) ErrorNameExists(distroName);
			}
			if (!File.Exists(tarPath)) Utils.Error($"File not found: {tarPath}.");
			if (Directory.Exists(targetPath)) Utils.Error($"Target directory already exists: {targetPath}.");

			Func<Stream, Stream> getTarStream = null;
			if (tarPath.EndsWith(".tar.gz", StringComparison.OrdinalIgnoreCase)) {
				getTarStream = s => new GZipInputStream(s);
			} else if (tarPath.EndsWith(".tar.xz", StringComparison.OrdinalIgnoreCase)) {
				// TODO: xz support.
				getTarStream = s => throw new NotImplementedException();
			} else {
				Utils.Error($"Unknown compression format \"{tarPath.Substring(tarPath.LastIndexOf('.'))}\".");
			}

			var targetRootPath = Path.Combine(targetPath, "rootfs");
			Utils.Log($"Creating the directory \"{targetRootPath}\".");
			Directory.CreateDirectory(targetRootPath);

			FileSystem.ExtractTar(getTarStream(File.OpenRead(tarPath)), targetRootPath);
			RegisterDistro(distroName, targetPath);
		}

		public static void RegisterDistro(string distroName, string installPath) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey != null) ErrorNameExists(distroName);
			}
			if (!Directory.Exists(installPath)) ErrorDirectoryExists(installPath);

			var id = Guid.NewGuid().ToString("B");
			Utils.Log($"Creating registry key {id} for the distribution \"{distroName}\".");
			using (var lxssKey = GetLxssKey(true))
			using (var distroKey = lxssKey.CreateSubKey(id)) {
				distroKey.SetValue("DistributionName", distroName);
				distroKey.SetValue("State", 1);
				distroKey.SetValue("Version", 1);
			}
			SetInstallationDirectory(distroName, Path.GetFullPath(installPath).TrimEnd('\\'));
		}

		public static void UninstallDistro(string distroName) {
			var installPath = GetInstallationDirectory(distroName);
			if (!Directory.Exists(installPath)) ErrorDirectoryExists(installPath);

			UnregisterDistro(distroName);
			FileSystem.DeleteDirectory(installPath);
		}

		public static void UnregisterDistro(string distroName) {
			string distroKeyName;

			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) ErrorNameNotFound(distroName);
				distroKeyName = Path.GetFileName(distroKey.Name);
			}

			using (var lxssKey = GetLxssKey(true)) {
				Utils.Log($"Deleting the registry key {distroKeyName}.");
				lxssKey.DeleteSubKey(distroKeyName);
			}
		}

		public static void MoveDistro(string distroName, string newPath) {
			if (Directory.Exists(newPath)) ErrorDirectoryExists(newPath);

			var newRootPath = Path.Combine(newPath, "rootfs");
			Utils.Log($"Creating the directory \"{newRootPath}\".");
			Directory.CreateDirectory(newRootPath);

			var oldPath = GetInstallationDirectory(distroName);
			FileSystem.CopyDirectory(Path.Combine(oldPath, "rootfs"), newRootPath);
			FileSystem.DeleteDirectory(oldPath);

			SetInstallationDirectory(distroName, newPath);
		}

		public static uint LaunchDistro(string distroName, string command, bool useCwd) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) ErrorNameNotFound(distroName);
			}

			Utils.Log($"Calling Win32 API {nameof(WslLaunchInteractive)}.");
			CheckWinApiResult(WslLaunchInteractive(distroName, command, useCwd, out var exitCode));
			Utils.Log($"Exit code is {exitCode}.");
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
