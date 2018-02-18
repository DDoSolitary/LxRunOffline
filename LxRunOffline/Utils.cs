using System;
using System.Diagnostics;
using System.IO;
using System.Security.Principal;
using Microsoft.Win32;

namespace LxRunOffline {
	static class Utils {
		static string promptAnswer = Environment.GetEnvironmentVariable("LXRUNOFFLINE_PROMPT_ANSWER");
		static bool showLog = Environment.GetEnvironmentVariable("LXRUNOFFLINE_VERBOSE") == "1";

		static void WriteLine(string output, ConsoleColor color) {
			Console.ForegroundColor = color;
			Console.Error.WriteLine(output);
			Console.ResetColor();
		}

		public static void Log(string output) {
#if !DEBUG
			if (!showLog) return;
#endif
			WriteLine($"[LOG] {output}", ConsoleColor.White);
		}

		public static void Warning(string output) {
			WriteLine($"[WARNING] {output}", ConsoleColor.Yellow);
		}

		public static void Error(string output) {
			WriteLine($"[ERROR] {output}", ConsoleColor.Red);
			Environment.Exit(1);
		}

		public static bool Prompt() {
			while (true) {
				Console.Write("Do you want to continue? (y/n): ");
				var keyChar = promptAnswer ?? Console.ReadKey().KeyChar.ToString();
				Console.WriteLine();
				if (keyChar == "y") return true;
				else if (keyChar == "n") return false;
				if (promptAnswer == null) Warning("Invalid input.");
				else Error($"Invalid environment variable: \"LXRUNOFFLINE_PROMPT_ANSWER={promptAnswer}\".");
			}
		}

		public static void CheckAdministrator() {
			var identity = WindowsIdentity.GetCurrent();
			var principal = new WindowsPrincipal(identity);

			if (!principal.IsInRole(WindowsBuiltInRole.Administrator)) return;

			Warning("You are running LxRunOffline with administrator privileges. It may cause problems.");
			if (!Prompt()) Error("User canceled the operation.");
		}

		public static void CheckCaseInsensitive() {
			const string regKey = @"HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Session Manager\Kernel";
			const string regValueName = "obcaseinsensitive";

			var value = Registry.GetValue(regKey, regValueName, 1);
			if (value is int intValue && intValue == 0) return;

			var startInfo = new ProcessStartInfo {
				FileName = "REG",
				Arguments = $"ADD \"{regKey}\" /v {regValueName} /t REG_DWORD /d 0 /f",
				Verb = "runas"
			};
			var errorMsg = $"Couldn't change the registry value: {regKey}\\{regValueName}.";
			try {
				using (var process = Process.Start(startInfo)) {
					process.WaitForExit();
					if (process.ExitCode != 0) Error($"{errorMsg} Exit code of reg.exe is {process.ExitCode}.");
				}
			} catch (Exception e) {
				Error($"{errorMsg} {e.Message}");
			}

			Error($"The registry value \"{regKey}\\{regValueName}\" has been set to \"0\"" +
				" to make sure this operation works properly." +
				" Please restart your system and then rerun the command.");
		}

		public static void CheckWslApi() {
			if (!File.Exists(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "wslapi.dll"))) {
				Error("wslapip.dll not found. Please enable \"Windows Subsystem for Linux\" in \"Control Panel > Turn Windows features on or off\".");
			}
		}

		public static void CheckCompatibility() {
			var version = Environment.OSVersion.Version;
			if (version.Major != 10 || version.Build < 16299) {
				Error("Windows 10 v1709 or later is required. Please update your system.");
			}

			if (!Environment.Is64BitOperatingSystem) {
				Error("A 64-bit Windows is required.");
			}
			if (!Environment.Is64BitProcess) {
				Error("Please run this program as a 64-bit process.");
			}
		}
	}
}
