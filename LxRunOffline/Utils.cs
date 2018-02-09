using System;
using System.Diagnostics;
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

		public static bool CheckAdministrator() {
			var identity = WindowsIdentity.GetCurrent();
			var principal = new WindowsPrincipal(identity);

			if (!principal.IsInRole(WindowsBuiltInRole.Administrator)) return true;

			Warning("You are running LxRunOffline with administrator privileges. It may cause problems.");
			return Prompt();
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
	}
}
