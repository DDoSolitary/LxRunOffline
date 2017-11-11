using System;
using System.Linq;

#if DEBUG
using System.Diagnostics;
#endif

namespace LxRunOffline
{
	class Program
	{
		static void Main(string[] args)
		{
			Action<int, int> checkArgLength = (min, max) => {
			#if DEBUG
				Debug.Assert(args.Length >= min && args.Length <= max);
			#else
				if (args.Length < min || args.Length > max) {
					Utils.Error("Invalid arguments.");
				}
			#endif
			};

			Action<DistroFlags> processFlag = flag => {
				if (args.Length == 3) {
					Console.WriteLine(Wsl.GetFlag(args[1], flag) ? 1 : 0);
				} else {
					checkArgLength(4, 4);
					Wsl.SetFlag(args[1], flag, args[3] != "0");
				}
			};

			checkArgLength(1, int.MaxValue);
			var action = args[0].ToLower();
			switch (action) {

			case "install":
				checkArgLength(4, 4);
				Wsl.InstallDistro(args[1], args[2], args[3]);
				break;

			case "register":
				checkArgLength(3, 3);
				Wsl.RegisterDistro(args[1], args[2]);
				break;

			case "uninstall":
				checkArgLength(2, 2);
				Wsl.UninstallDistro(args[1]);
				break;

			case "unregister":
				checkArgLength(2, 2);
				Wsl.UnregisterDistro(args[1]);
				break;

			case "move":
				checkArgLength(3, 3);
				Wsl.MoveDistro(args[1], args[2]);
				break;

			case "run":
				checkArgLength(2, 3);
				string cmd = "/bin/bash --login";
				if (args.Length == 3) cmd = args[2];
				Environment.Exit((int)Wsl.LaunchDistro(args[1], cmd));
				break;

			case "list":
				checkArgLength(1, 1);
				Console.WriteLine(string.Join("\n", Wsl.ListDistros().ToArray()));
				break;

			case "default":
				if (args.Length == 1) {
					Console.WriteLine(Wsl.GetDefaultDistro());
				} else {
					checkArgLength(2, 2);
					Wsl.SetDefaultDistro(args[1]);
				}
				break;

			case "config":
				checkArgLength(3, int.MaxValue);

				switch (args[2]) {

				case "dir":
					checkArgLength(3, 3);
					Console.WriteLine(Wsl.GetInstallationDirectory(args[1]));
					break;

				case "env":
					if (args.Length == 3) {
						var envVars = Wsl.GetDefaultEnvironment(args[1]);
						if (envVars != null) Console.WriteLine(string.Join("\n", envVars));
					} else {
						var envVars = new string[args.Length - 3];
						Array.Copy(args, 3, envVars, 0, envVars.Length);
						Wsl.SetDefaultEnvironment(args[1], envVars);
					}
					break;

				case "uid":
					if (args.Length == 3) {
						Console.WriteLine(Wsl.GetDefaultUid(args[1]));
					} else {
						checkArgLength(4, 4);
						if (!int.TryParse(args[3], out var uid)) Utils.Error("Invalid arguments");
						Wsl.SetDefaultUid(args[1], uid);
					}
					break;

				case "kernelcmd":
					if (args.Length == 3) {
						Console.WriteLine(Wsl.GetKernelCommandLine(args[1]));
					} else {
						checkArgLength(4, 4);
						Wsl.SetKernelCommandLine(args[1], args[3]);
					}
					break;

				case "interop":
					processFlag(DistroFlags.EnableInterop);
					break;
				case "appendpath":
					processFlag(DistroFlags.AppendNtPath);
					break;
				case "mount":
					processFlag(DistroFlags.EnableDriveMounting);
					break;

				default:
					Utils.Error("Invalid arguments.");
					break;
				}

				break;
			
			default:
				Utils.Error("Invalid arguments.");
				break;
			}
		}
	}
}
