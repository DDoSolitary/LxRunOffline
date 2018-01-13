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

		static void DeleteDirectory(string path) {
			Utils.Log($"Deleting the directory \"{path}\".");
			var retryCount = DeletionRetryCount;
			while (true) {
				retryCount--;
				try {
					Directory.Delete(path, true);
					return;
				} catch (Exception e) {
					Utils.Warning($"Couldn't delete the directory \"{path}\": {e.Message}");
					if (retryCount == 0) {
						Utils.Warning($"You may have to delete it manually.");
					} else {
						Utils.Warning($"Retrying.");
					}
				}
			}
		}

		static bool MoveDirectory(string oldPath, string newPath) {
			Utils.Log($"Moving the directory \"{oldPath}\" to \"{newPath}\".");
			try {
				Directory.Move(oldPath, newPath);
				return true;
			} catch (Exception e) {
				Utils.Warning($"Couldn't move the directory \"{oldPath}\" to \"{newPath}\": {e.Message}");
				Utils.Warning("It is still possible to move the directory using \"robocopy\", but it will cause problems including loss of directory permission and some files. You may have to fix them manually later.");
				if (!Utils.Prompt()) return false;
			}

			var startInfo = new ProcessStartInfo {
				FileName = "robocopy",
				Arguments = $"{RobocopyArguments} \"{Path.GetFullPath(oldPath)}\" \"{Path.GetFullPath(newPath)}\"",
				Verb = "runas"
			};
			Utils.Log($"Starting the process: {startInfo.FileName} {startInfo.Arguments}");
			using (var process = Process.Start(startInfo)) {
				process.WaitForExit();
				if (process.ExitCode > 1) {
					Utils.Warning($"robocopy exited with a non-successful code: {process.ExitCode}.");
					return false;
				}
			}
			DeleteDirectory(oldPath);
			return true;
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

		public static void InstallDistro(string distroName, string tarGzPath, string targetPath) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey != null) ErrorNameExists(distroName);
			}
			if (!File.Exists(tarGzPath)) Utils.Error($"File not found: {tarGzPath}.");
			if (Directory.Exists(targetPath)) Utils.Error($"Target directory already exists: {targetPath}.");

			string tmpRootPath = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "rootfs");
			if (Directory.Exists(tmpRootPath))
				Utils.Error($"The \"rootfs\" directory already exists in the directory containing the program: {tmpRootPath}. It may be caused by a crash of this program. Please delete it manually.");

			Utils.Log($"Calling Win32 API {nameof(WslWinApi.WslRegisterDistribution)}.");
			CheckWinApiResult(WslWinApi.WslRegisterDistribution(distroName, Path.GetFullPath(tarGzPath)));

			Utils.Log($"Creating the directory \"{targetPath}\".");
			Directory.CreateDirectory(targetPath);

			if (!MoveDirectory(tmpRootPath, Path.Combine(targetPath, "rootfs"))) {
				Utils.Warning("Directory moving failed, cleaning up.");
				UnregisterDistro(distroName);
				DeleteDirectory(targetPath);
				DeleteDirectory(tmpRootPath);
				Environment.Exit(1);
			}

			SetInstallationDirectory(distroName, targetPath);
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
			DeleteDirectory(installPath);
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

			var oldPath = GetInstallationDirectory(distroName);
			if (!MoveDirectory(oldPath, newPath)) {
				Utils.Error("Directory moving failed.");
			}
			SetInstallationDirectory(distroName, newPath);
		}

		public static uint LaunchDistro(string distroName, string command, bool useCwd) {
			using (var distroKey = FindDistroKey(distroName)) {
				if (distroKey == null) ErrorNameNotFound(distroName);
			}

			Utils.Log($"Calling Win32 API {nameof(WslWinApi.WslLaunchInteractive)}.");
			CheckWinApiResult(WslWinApi.WslLaunchInteractive(distroName, command, useCwd, out var exitCode));
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
