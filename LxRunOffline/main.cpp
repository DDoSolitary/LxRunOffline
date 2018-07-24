#include "stdafx.h"
#include "error.h"
#include "fs.h"
#include "reg.h"
#include "utils.h"

namespace po = boost::program_options;

void check_compatibility() {
#ifndef LXRUNOFFLINE_NO_WIN10
	OSVERSIONINFO ver;
	ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
#pragma warning(disable:4996)
	if (!GetVersionEx(&ver)) {
#pragma warning(default:4996)
		throw error_other(err_get_version, {});
	}
	if (ver.dwBuildNumber < 17134) {
		throw error_other(err_version_old, { L"1803",L"17134" });
	}
#endif
}

#ifdef __MINGW32__
extern "C"
#endif
int wmain(int argc, wchar_t **argv) {
	wstr name;
	po::options_description desc("Options");
	desc.add_options()(",n", po::wvalue<wstr>(&name)->required(), "Name of the distribution");
	po::variables_map vm;
	auto parse_args = [&]() {
		po::store(po::parse_command_line(argc - 1, argv + 1, desc), vm);
		po::notify(vm);
	};

	try {
		check_compatibility();
		if (argc < 2) {
			throw error_other(err_no_action, {});
#ifdef LXRUNOFFLINE_VERSION
		} else if (!wcscmp(argv[1], L"version")) {
			std::wcout << L"LxRunOffline " << LXRUNOFFLINE_VERSION << std::endl;
#endif
		} else if (!wcscmp(argv[1], L"list")) {
			for (const auto &s : list_distros()) {
				std::wcout << s << std::endl;
			}
		} else if (!wcscmp(argv[1], L"get-default")) {
			std::wcout << get_default_distro() << std::endl;
		} else if (!wcscmp(argv[1], L"set-default")) {
			parse_args();
			set_default_distro(name);
		} else if (!wcscmp(argv[1], L"install")) {
			wstr dir, file, root;
			desc.add_options()
				(",d", po::wvalue<wstr>(&dir)->required(), "The directory to install the distribution into.")
				(",f", po::wvalue<wstr>(&file)->required(), "The tar file containing the root filesystem of the distribution to be installed.")
				(",r", po::wvalue<wstr>(&root), "The directory in the tar file to extract. This argument is optional");
			parse_args();
			register_distro(name, dir);
			extract_archive(file, root, dir);
		} else if (!wcscmp(argv[1], L"uninstall")) {
			parse_args();
			auto dir = get_distro_dir(name);
			unregister_distro(name);
			delete_directory(dir);
		} else if (!wcscmp(argv[1], L"register")) {
			wstr dir;
			desc.add_options()(",d", po::wvalue<wstr>(&dir)->required(), "The directory containing the distribution.");
			parse_args();
			register_distro(name, dir);
		} else if (!wcscmp(argv[1], L"unregister")) {
			parse_args();
			unregister_distro(name);
		} else if (!wcscmp(argv[1], L"move")) {
			wstr dir;
			desc.add_options()(",d", po::wvalue<wstr>(&dir)->required(), "The directory to move the distribution to.");
			parse_args();
			move_directory(get_distro_dir(name), dir);
			set_distro_dir(name, dir);
		} else if (!wcscmp(argv[1], L"duplicate")) {
			wstr new_name, dir;
			desc.add_options()(",d", po::wvalue<wstr>(&dir)->required(), "The directory to copy the distribution to.");
			desc.add_options()(",N", po::wvalue<wstr>(&new_name)->required(), "Name of the new distribution.");
			parse_args();
			duplicate_distro(name, new_name, dir);
			copy_directory(get_distro_dir(name), dir);
		} else if (!wcscmp(argv[1], L"run")) {
			wstr cmd;
			bool no_cwd;
			desc.add_options()(",c", po::wvalue<wstr>(&cmd)->default_value(L"/bin/bash --login", "/bin/bash --login"), "The command to run.");
			desc.add_options()(",w", po::bool_switch(&no_cwd), "Don't use the working directory in Windows for the Linux process.");
			parse_args();
			auto hw = LoadLibraryEx(L"wslapi.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (hw == INVALID_HANDLE_VALUE) throw error_win32_last(err_no_wslapi, {});
			auto launch = (HRESULT(__stdcall *)(PCWSTR, PCWSTR, BOOL, DWORD *))GetProcAddress(hw, "WslLaunchInteractive");
			if (!launch) throw error_win32_last(err_no_wslapi, {});
			DWORD code;
			auto hr = launch(name.c_str(), cmd.c_str(), !no_cwd, &code);
			if (FAILED(hr)) throw error_hresult(err_launch_distro, { name }, hr);
			return code;
		} else if (!wcscmp(argv[1], L"get-dir")) {
			parse_args();
			std::wcout << get_distro_dir(name);
		} else if (!wcscmp(argv[1], L"get-env")) {
			parse_args();
			for (const auto &s : get_distro_env(name)) {
				std::wcout << s << std::endl;
			}
		} else if (!wcscmp(argv[1], L"set-env")) {
			std::vector<wstr> env;
			desc.add_options()(",v", po::wvalue<std::vector<wstr>>(&env)->required(), "Environment variables to be set. This argument can be specified multiple times.");
			parse_args();
			set_distro_env(name, env);
		} else if (!wcscmp(argv[1], L"get-uid")) {
			parse_args();
			std::wcout << get_distro_uid(name);
		} else if (!wcscmp(argv[1], L"set-uid")) {
			uint32_t uid;
			desc.add_options()(",v", po::wvalue<uint32_t>(&uid)->required(), "UID to be set.");
			parse_args();
			set_distro_uid(name, uid);
		} else if (!wcscmp(argv[1], L"get-kernelcmd")) {
			parse_args();
			std::wcout << get_distro_kernel_cmd(name);
		} else if (!wcscmp(argv[1], L"set-kernelcmd")) {
			wstr cmd;
			desc.add_options()(",v", po::wvalue<wstr>(&cmd)->required(), "Kernel command line to be set.");
			parse_args();
			set_distro_kernel_cmd(name, cmd);
		} else if (!wcscmp(argv[1], L"get-flags")) {
			parse_args();
			std::wcout << get_distro_flags(name);
		} else if (!wcscmp(argv[1], L"set-flags")) {
			uint32_t flags;
			desc.add_options()(",v", po::wvalue<uint32_t>(&flags)->required(), "Flags to be set.");
			parse_args();
			set_distro_flags(name, flags);
		} else {
			throw error_other(err_invalid_action, { argv[1] });
		}
	} catch (const err &e) {
		log_error(e.format());
		if (e.msg_code == err_set_cs && e.err_code == HRESULT_FROM_NT(STATUS_ACCESS_DENIED)) {
			log_warning(L"You may have run into a known bug of Windows (https://github.com/Microsoft/WSL/issues/3304). Please try giving \"Delete subfolders or files\" permission of the target directory to the current user or simply running this tool with admin privilege.");
		} else if (e.msg_code == err_no_action || e.msg_code == err_invalid_action) {
			std::wcerr
				<< L"Supported actions are:" << std::endl
				<< L"    list           List all installed distributions." << std::endl
				<< L"    get-default    Get the default distribution, which is used by bash.exe." << std::endl
				<< L"    set-default    Set the default distribution, which is used by bash.exe." << std::endl
				<< L"    install        Install a new distribution." << std::endl
				<< L"    uninstall      Uninstall a distribution." << std::endl
				<< L"    register       Register an existing installation directory." << std::endl
				<< L"    unregister     Unregister a distribution but not delete the installation directory." << std::endl
				<< L"    move           Move a distribution to a new directory." << std::endl
				<< L"    duplicate      Duplicate an existing distribution in a new directory." << std::endl
				<< L"    run            Run a command in a distribution." << std::endl
				<< L"    get-dir        Get the installation directory of a distribution." << std::endl
				<< L"    get-env        Get the default environment variables of a distribution." << std::endl
				<< L"    set-env        Set the default environment variables of a distribution." << std::endl
				<< L"    get-uid        Get the UID of the default user of a distribution." << std::endl
				<< L"    set-uid        Set the UID of the default user of a distribution." << std::endl
				<< L"    get-kernelcmd  Get the default kernel command line of a distribution." << std::endl
				<< L"    set-kernelcmd  Set the default kernel command line of a distribution." << std::endl
				<< L"    get-flags      Get some flags of a distribution. See https://msdn.microsoft.com/en-us/library/windows/desktop/mt826872(v=vs.85).aspx for details." << std::endl
				<< L"    set-flags      Set some flags of a distribution. See https://msdn.microsoft.com/en-us/library/windows/desktop/mt826872(v=vs.85).aspx for details." << std::endl;
#ifdef LXRUNOFFLINE_VERSION
			std::wcerr << L"    version        Get version information about this LxRunOffline.exe." << std::endl;
#endif
		}
		return 1;
	} catch (const po::error &e) {
		std::wstringstream ss;
		ss << e.what();
		log_error(ss.str());
		std::cout << std::endl << desc;
	}
	return 0;
}
