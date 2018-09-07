#include "stdafx.h"
#include "error.h"
#include "fs.h"
#include "reg.h"
#include "shortcut.h"
#include "utils.h"

namespace po = boost::program_options;

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
#ifndef LXRUNOFFLINE_NO_WIN10
		if (win_build < 17134) {
			throw error_other(err_version_old, { L"1803",L"17134" });
		}
#endif
		if (argc < 2) {
			throw error_other(err_no_action, {});
#ifdef LXRUNOFFLINE_VERSION
		} else if (!wcscmp(argv[1], L"version")) {
			std::wcout << L"LxRunOffline " << LXRUNOFFLINE_VERSION << std::endl;
#endif
		} else if (!wcscmp(argv[1], L"l") || !wcscmp(argv[1], L"list")) {
			for (crwstr s : list_distros()) {
				std::wcout << s << std::endl;
			}
		} else if (!wcscmp(argv[1], L"gd") || !wcscmp(argv[1], L"get-default")) {
			std::wcout << get_default_distro() << std::endl;
		} else if (!wcscmp(argv[1], L"sd") || !wcscmp(argv[1], L"set-default")) {
			parse_args();
			set_default_distro(name);
		} else if (!wcscmp(argv[1], L"i") || !wcscmp(argv[1], L"install")) {
			wstr dir, file, root, conf_path;
			bool shortcut;
			desc.add_options()
				(",d", po::wvalue<wstr>(&dir)->required(), "The directory to install the distribution into.")
				(",f", po::wvalue<wstr>(&file)->required(), "The tar file containing the root filesystem of the distribution to be installed.")
				(",r", po::wvalue<wstr>(&root), "The directory in the tar file to extract. This argument is optional.")
				(",c", po::wvalue<wstr>(&conf_path), "The config file to use. This argument is optional.")
				(",s", po::bool_switch(&shortcut), "Create a shortcut for this distribution on Desktop.");
			parse_args();
			reg_config conf;
			if (!conf_path.empty()) conf.load_file(conf_path);
			register_distro(name, dir);
			conf.configure_distro(name, config_all);
			extract_archive(file, root, dir);
			auto dp = unique_val<wchar_t *>([&](wchar_t *&s) {
				auto hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, 0, &s);
				if (FAILED(hr)) throw error_hresult(err_create_shortcut, {}, hr);
			}, &CoTaskMemFree);
			if (shortcut) create_shortcut(name, dp.val + (L'\\' + name + L".lnk"), L"");
		} else if (!wcscmp(argv[1], L"ui") || !wcscmp(argv[1], L"uninstall")) {
			parse_args();
			auto dir = get_distro_dir(name);
			unregister_distro(name);
			delete_directory(dir);
		} else if (!wcscmp(argv[1], L"rg") || !wcscmp(argv[1], L"register")) {
			wstr dir, conf_path;
			desc.add_options()
				(",d", po::wvalue<wstr>(&dir)->required(), "The directory containing the distribution.")
				(",c", po::wvalue<wstr>(&conf_path), "The config file to use. This argument is optional.");
			parse_args();
			reg_config conf;
			if (!conf_path.empty()) conf.load_file(conf_path);
			register_distro(name, dir);
			conf.configure_distro(name, config_all);
		} else if (!wcscmp(argv[1], L"ur") || !wcscmp(argv[1], L"unregister")) {
			parse_args();
			unregister_distro(name);
		} else if (!wcscmp(argv[1], L"m") || !wcscmp(argv[1], L"move")) {
			wstr dir;
			desc.add_options()(",d", po::wvalue<wstr>(&dir)->required(), "The directory to move the distribution to.");
			parse_args();
			move_directory(get_distro_dir(name), dir);
			set_distro_dir(name, dir);
		} else if (!wcscmp(argv[1], L"d") || !wcscmp(argv[1], L"duplicate")) {
			wstr new_name, dir, conf_path;
			desc.add_options()
				(",d", po::wvalue<wstr>(&dir)->required(), "The directory to copy the distribution to.")
				(",N", po::wvalue<wstr>(&new_name)->required(), "Name of the new distribution.")
				(",c", po::wvalue<wstr>(&conf_path), "The config file to use. This argument is optional.");
			parse_args();
			reg_config conf;
			conf.load_distro(name, config_all);
			if (!conf_path.empty()) conf.load_file(conf_path);
			register_distro(new_name, dir);
			conf.configure_distro(new_name, config_all);
			copy_directory(get_distro_dir(name), dir);
		} else if (!wcscmp(argv[1], L"r") || !wcscmp(argv[1], L"run")) {
			wstr cmd;
			bool no_cwd;
			desc.add_options()
				(",c", po::wvalue<wstr>(&cmd)->default_value(L"/bin/bash --login", "/bin/bash --login"), "The command to run.")
				(",w", po::bool_switch(&no_cwd), "Don't use the working directory in Windows for the Linux process.");
			parse_args();
			auto hw = LoadLibraryEx(L"wslapi.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (hw == INVALID_HANDLE_VALUE) throw error_win32_last(err_no_wslapi, {});
			auto launch = (HRESULT(__stdcall *)(PCWSTR, PCWSTR, BOOL, DWORD *))GetProcAddress(hw, "WslLaunchInteractive");
			if (!launch) throw error_win32_last(err_no_wslapi, {});
			DWORD code;
			auto hr = launch(name.c_str(), cmd.c_str(), !no_cwd, &code);
			if (FAILED(hr)) throw error_hresult(err_launch_distro, { name }, hr);
			return code;
		} else if (!wcscmp(argv[1], L"gd") || !wcscmp(argv[1], L"get-dir")) {
			parse_args();
			std::wcout << get_distro_dir(name);
		} else if (!wcscmp(argv[1], L"ge") || !wcscmp(argv[1], L"get-env")) {
			parse_args();
			reg_config conf;
			conf.load_distro(name, config_env);
			for (crwstr s : conf.env) {
				std::wcout << s << std::endl;
			}
		} else if (!wcscmp(argv[1], L"se") || !wcscmp(argv[1], L"set-env")) {
			reg_config conf;
			desc.add_options()(",v", po::wvalue<std::vector<wstr>>(&conf.env)->required(), "Environment variables to be set. This argument can be specified multiple times.");
			parse_args();
			conf.configure_distro(name, config_env);
		} else if (!wcscmp(argv[1], L"ae") || !wcscmp(argv[1], L"add-env")) {
			wstr env;
			bool force;
			desc.add_options()
				(",v", po::wvalue<wstr>(&env)->required(), "The environment variable to add.")
				(",f", po::bool_switch(&force), "Overwrite if the environment variable already exists.");
			parse_args();
			auto p = env.find(L'=');
			if (p == wstr::npos) throw error_other(err_invalid_env, { env });
			auto env_name = env.substr(0, p + 1);
			reg_config conf;
			conf.load_distro(name, config_env);
			auto it = std::find_if(conf.env.begin(), conf.env.end(), [&](crwstr s) {
				return !s.compare(0, env_name.size(), env_name);
			});
			if (it != conf.env.end()) {
				if (force) conf.env.erase(it);
				else throw error_other(err_env_exists, { *it });
			}
			conf.env.push_back(env);
			conf.configure_distro(name, config_env);
		} else if (!wcscmp(argv[1], L"re") || !wcscmp(argv[1], L"remove-env")) {
			wstr env_name;
			desc.add_options()(",v", po::wvalue<wstr>(&env_name)->required(), "Name of the environment variable to remove.");
			parse_args();
			reg_config conf;
			conf.load_distro(name, config_env);
			auto it = std::find_if(conf.env.begin(), conf.env.end(), [&](crwstr s) {
				return !s.compare(0, env_name.size() + 1, env_name + L"=");
			});
			if (it == conf.env.end()) throw error_other(err_env_not_found, { env_name });
			conf.env.erase(it);
			conf.configure_distro(name, config_env);
		} else if (!wcscmp(argv[1], L"gu") || !wcscmp(argv[1], L"get-uid")) {
			parse_args();
			reg_config conf;
			conf.load_distro(name, config_uid);
			std::wcout << conf.uid;
		} else if (!wcscmp(argv[1], L"su") || !wcscmp(argv[1], L"set-uid")) {
			reg_config conf;
			desc.add_options()(",v", po::wvalue<uint32_t>(&conf.uid)->required(), "UID to be set.");
			parse_args();
			conf.configure_distro(name, config_uid);
		} else if (!wcscmp(argv[1], L"gk") || !wcscmp(argv[1], L"get-kernelcmd")) {
			parse_args();
			reg_config conf;
			conf.load_distro(name, config_kernel_cmd);
			std::wcout << conf.kernel_cmd;
		} else if (!wcscmp(argv[1], L"sk") || !wcscmp(argv[1], L"set-kernelcmd")) {
			reg_config conf;
			desc.add_options()(",v", po::wvalue<wstr>(&conf.kernel_cmd)->required(), "Kernel command line to be set.");
			parse_args();
			conf.configure_distro(name, config_kernel_cmd);
		} else if (!wcscmp(argv[1], L"gf") || !wcscmp(argv[1], L"get-flags")) {
			parse_args();
			reg_config conf;
			conf.load_distro(name, config_flags);
			std::wcout << conf.flags;
		} else if (!wcscmp(argv[1], L"sf") || !wcscmp(argv[1], L"set-flags")) {
			reg_config conf;
			desc.add_options()(",v", po::wvalue<uint32_t>(&conf.flags)->required(), "Flags to be set.");
			parse_args();
			conf.configure_distro(name, config_flags);
		} else if (!wcscmp(argv[1], L"s") || !wcscmp(argv[1], L"shortcut")) {
			wstr fp, ip;
			desc.add_options()
				(",f", po::wvalue<wstr>(&fp)->required(), "Path to the shortcut to be created, including the \".lnk\" suffix.")
				(",i", po::wvalue<wstr>(&ip), "Path to the icon file for the shortcut. This argument is optional.");
			parse_args();
			create_shortcut(name, fp, ip);
		} else if (!wcscmp(argv[1], L"ec") || !wcscmp(argv[1], L"export-config")) {
			wstr file;
			desc.add_options()(",f", po::wvalue<wstr>(&file)->required(), "Path to the XML file to export to.");
			parse_args();
			reg_config conf;
			conf.load_distro(name, config_all);
			conf.save_file(file);
		} else if (!wcscmp(argv[1], L"ic") || !wcscmp(argv[1], L"import-config")) {
			wstr file;
			desc.add_options()(",f", po::wvalue<wstr>(&file)->required(), "The XML file to import from.");
			parse_args();
			reg_config conf;
			conf.load_file(file);
			conf.configure_distro(name, config_all);
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
				<< L"    l, list            List all installed distributions." << std::endl
				<< L"    gd, get-default    Get the default distribution, which is used by bash.exe." << std::endl
				<< L"    sd, set-default    Set the default distribution, which is used by bash.exe." << std::endl
				<< L"    i, install         Install a new distribution." << std::endl
				<< L"    ui, uninstall      Uninstall a distribution." << std::endl
				<< L"    rg, register       Register an existing installation directory." << std::endl
				<< L"    ur, unregister     Unregister a distribution but not delete the installation directory." << std::endl
				<< L"    m, move            Move a distribution to a new directory." << std::endl
				<< L"    d, duplicate       Duplicate an existing distribution in a new directory." << std::endl
				<< L"    r, run             Run a command in a distribution." << std::endl
				<< L"    gd, get-dir        Get the installation directory of a distribution." << std::endl
				<< L"    ge, get-env        Get the default environment variables of a distribution." << std::endl
				<< L"    se, set-env        Set the default environment variables of a distribution." << std::endl
				<< L"    ae, add-env        Add to the default environment variables of a distribution." << std::endl
				<< L"    re, remove-env     Remove from the default environment variables of a distribution." << std::endl
				<< L"    gu, get-uid        Get the UID of the default user of a distribution." << std::endl
				<< L"    su, set-uid        Set the UID of the default user of a distribution." << std::endl
				<< L"    gk, get-kernelcmd  Get the default kernel command line of a distribution." << std::endl
				<< L"    sk, set-kernelcmd  Set the default kernel command line of a distribution." << std::endl
				<< L"    gf, get-flags      Get some flags of a distribution. See https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/wslapi/ne-wslapi-wsl_distribution_flags for details." << std::endl
				<< L"    sf, set-flags      Set some flags of a distribution. See https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/wslapi/ne-wslapi-wsl_distribution_flags for details." << std::endl
				<< L"    s, shortcut        Create a shortcut to launch a distribution." << std::endl
				<< L"    ec, export-config  Export configuration of a distribution to an XML file." << std::endl
				<< L"    ic, import-config  Import configuration of a distribution from an XML file." << std::endl;
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
