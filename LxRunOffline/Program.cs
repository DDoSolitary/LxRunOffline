using System;
using System.Linq;
using CommandLine;

namespace LxRunOffline {
	[Verb("list", HelpText = "List all installed distributions.")]
	class ListOptions {}

	[Verb("default", HelpText = "Get or set the default distribution, which is used by bash.exe.")]
	class DefaultOptions {
		[Option('n', HelpText = "Name of the distribution to be set as default.")]
		public string Name { get; set; }
	}

	[Verb("install", HelpText = "Install a new distribution.")]
	class InstallOptions {
		[Option('n', HelpText = "Name used to register the distribution.", Required = true)]
		public string Name { get; set; }

		[Option('f', HelpText = "A file containing the root filesystem of the distribution to be installed. See project's Wiki on GitHub for how to create it.", Required = true)]
		public string TarGzFile { get; set; }

		[Option('d', HelpText = "The directory to install the distribution into. It should not exist and will be created automatically.", Required = true)]
		public string TargetDirectory { get; set; }

		[Option('r', HelpText = "The root directory in the tar file to extract. The default is to extract the whole tar file.")]
		public string TarRootDirectory { get; set; }
	}

	[Verb("register", HelpText = "Register an existing installation directory.")]
	class RegisterOptions {
		[Option('n', HelpText = "Name used to register the distribution.", Required = true)]
		public string Name { get; set; }

		[Option('d', HelpText = "The directory containing the distribution to be registered.", Required = true)]
		public string InstallationDirectory { get; set; }
	}

	[Verb("uninstall", HelpText = "Uninstall a distribution")]
	class UninstallOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }
	}

	[Verb("unregister", HelpText = "Unregister a distribution but not delete the directory containing it.")]
	class UnregisterOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }
	}

	[Verb("move", HelpText = "Move a distribution to a new directory.")]
	class MoveOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }

		[Option('d', HelpText = "The directory to move the distribution to. It should not exist and will be created automatically.", Required = true)]
		public string TargetDirectory { get; set; }
	}

	[Verb("run", HelpText = "Run a command in a distribution.")]
	class RunOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }

		[Option('c', HelpText = "The command to be run.", Default = "/bin/bash --login")]
		public string Command { get; set; }

		[Option('w', HelpText = "Don't use the working directory in Windows for the Linux program.", Default = false)]
		public bool NoCwd { get; set; }
	}

	[Verb("dir", HelpText = "Get the installation directory of a distribution.")]
	class DirOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }
	}

	[Verb("config-env", HelpText = "Get or set the default environment variables of a distribution.")]
	class ConfigEnvOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }

		[Option('a', HelpText = "Environment variable to add. (NAME=value)", SetName = "add")]
		public string VariableToAdd { get; set; }

		[Option('r', HelpText = "Name of the environment variable to remove.", SetName = "remove")]
		public string VariableToRemove { get; set; }
	}

	[Verb("config-uid", HelpText = "Get or set the UID of the default user of a distribution.")]
	class ConfigUidOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }

		[Option('v', HelpText = "UID to be set.")]
		public int? Uid { get; set; }
	}

	[Verb("config-kernelcmd", HelpText = "Get or set the default kernel command line of a distribution.")]
	class ConfigKernelCmdOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }

		[Option('v', HelpText = "Kernel command line to be set.")]
		public string Commandline { get; set; }
	}

	[Verb("config-flag", HelpText = "Get or set some flags of a distribution. See https://msdn.microsoft.com/en-us/library/windows/desktop/mt826872(v=vs.85).aspx for details.")]
	class ConfigFlagOptions {
		[Option('n', HelpText = "Name of the distribution.", Required = true)]
		public string Name { get; set; }

		[Option('f', HelpText = "Name of the flag to get or set.", Required = true)]
		public string FlagName { get; set; }

		[Option('v', HelpText = "Flag value to be set. Zero for off while non-zero for on.")]
		public int? Flag { get; set; }
	}

	class Program {
		static int Main(string[] args) {
			if (!Utils.CheckAdministrator()) return 0;

			return Parser.Default.ParseArguments<ListOptions, DefaultOptions, InstallOptions, RegisterOptions, UninstallOptions, UnregisterOptions, MoveOptions, RunOptions, DirOptions, ConfigEnvOptions, ConfigUidOptions, ConfigKernelCmdOptions, ConfigFlagOptions>(args).MapResult(
				(ListOptions opts) => {
					Console.WriteLine(string.Join("\n", Wsl.ListDistros().ToArray()));
					return 0;
				},
				(DefaultOptions opts) => {
					if (opts.Name == null) {
						Console.WriteLine(Wsl.GetDefaultDistro());
					} else {
						Wsl.SetDefaultDistro(opts.Name);
					}
					return 0;
				},
				(InstallOptions opts) => {
					if (!Utils.CheckCaseInsensitive()) return 0;

					Wsl.InstallDistro(opts.Name, opts.TarGzFile, opts.TarRootDirectory, opts.TargetDirectory);
					return 0;
				},
				(RegisterOptions opts) => {
					Wsl.RegisterDistro(opts.Name, opts.InstallationDirectory);
					return 0;
				},
				(UninstallOptions opts) => {
					Wsl.UninstallDistro(opts.Name);
					return 0;
				},
				(UnregisterOptions opts) => {
					Wsl.UnregisterDistro(opts.Name);
					return 0;
				},
				(MoveOptions opts) => {
					if (!Utils.CheckCaseInsensitive()) return 0;

					Wsl.MoveDistro(opts.Name, opts.TargetDirectory);
					return 0;
				},
				(RunOptions opts) => {
					return (int)Wsl.LaunchDistro(opts.Name, opts.Command, !opts.NoCwd);
				},
				(DirOptions opts) => {
					Console.WriteLine(Wsl.GetInstallationDirectory(opts.Name));
					return 0;
				},
				(ConfigEnvOptions opts) => {
					var envVars = Wsl.GetDefaultEnvironment(opts.Name).ToList();
					if (opts.VariableToAdd != null) {
						if (!opts.VariableToAdd.Contains('=')) {
							Utils.Error($"Environment variable must contain \"=\": \"{opts.VariableToAdd}\".");
						}
						envVars.Add(opts.VariableToAdd);
						Wsl.SetDefaultEnvironment(opts.Name, envVars.ToArray());
					} else if (opts.VariableToRemove != null) {
						if (opts.VariableToRemove.Contains('=')) {
							Utils.Error($"Environment variable name should not contain \"=\": \"{opts.VariableToRemove}\".");
						}
						var newEnvVars = envVars.Where(s => !s.StartsWith($"{opts.VariableToRemove}="));
						if (envVars.Count == newEnvVars.Count()) {
							Utils.Error($"Environment variable not found: {opts.VariableToRemove}.");
						}
						Wsl.SetDefaultEnvironment(opts.Name, newEnvVars.ToArray());
					} else {
						Console.Write(string.Join("\n", envVars));
					}
					return 0;
				},
				(ConfigUidOptions opts) => {
					if (opts.Uid == null) {
						Console.WriteLine(Wsl.GetDefaultUid(opts.Name));
					} else {
						Wsl.SetDefaultUid(opts.Name, opts.Uid.Value);
					}
					return 0;
				},
				(ConfigKernelCmdOptions opts) => {
					if (opts.Commandline == null) {
						Console.WriteLine(Wsl.GetKernelCommandLine(opts.Name));
					} else {
						Wsl.SetKernelCommandLine(opts.Name, opts.Commandline);
					}
					return 0;
				},
				(ConfigFlagOptions opts) => {
					switch (opts.FlagName) {
					case "WSL_DISTRIBUTION_FLAGS_ENABLE_INTEROP":
						if (opts.Flag == null) {
							Console.WriteLine(Wsl.GetFlag(opts.Name, DistroFlags.EnableInterop) ? 1 : 0);
						} else {
							Wsl.SetFlag(opts.Name, DistroFlags.EnableInterop, opts.Flag != 0);
						}
						return 0;
					case "WSL_DISTRIBUTION_FLAGS_APPEND_NT_PATH":
						if (opts.Flag == null) {
							Console.WriteLine(Wsl.GetFlag(opts.Name, DistroFlags.AppendNtPath) ? 1 : 0);
						} else {
							Wsl.SetFlag(opts.Name, DistroFlags.AppendNtPath, opts.Flag != 0);
						}
						return 0;
					case "WSL_DISTRIBUTION_FLAGS_ENABLE_DRIVE_MOUNTING":
						if (opts.Flag == null) {
							Console.WriteLine(Wsl.GetFlag(opts.Name, DistroFlags.EnableDriveMounting) ? 1 : 0);
						} else {
							Wsl.SetFlag(opts.Name, DistroFlags.EnableDriveMounting, opts.Flag != 0);
						}
						return 0;
					default:
						Utils.Error("Flag name not found.");
						return 1; // Never reached.
					}
				},
				errs => 1
			);
		}
	}
}
