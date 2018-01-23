using System;
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

		public static bool CheckAdministrator()
		{
			var identity = WindowsIdentity.GetCurrent();
			var principal = new WindowsPrincipal(identity);

			if (!principal.IsInRole(WindowsBuiltInRole.Administrator)) return true;

			Warning("You are running LxRunOffline with administrator privileges. It may cause problems.");
			return Prompt();
		}

		public static bool CheckCaseInsensitive() {
			const string regKey = @"HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Session Manager\Kernel";
			const string regValueName = "obcaseinsensitive";

			var value = Registry.GetValue(regKey, regValueName, 1);
			if (value is int intValue && intValue == 0) return true;

			Warning($"The registry value \"{regKey}\\{regValueName}\" is NOT set to \"0\", which will cause problems. Please set it to \"0\" and restart your system.");
			return Prompt();
		}
	}
}
