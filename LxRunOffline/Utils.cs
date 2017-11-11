using System;

namespace LxRunOffline {
	class Utils {
		public static void Error(string output) {
			Console.Error.WriteLine(output);
			Environment.Exit(1);
		}
	}
}