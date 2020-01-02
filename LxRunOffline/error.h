#pragma once
#include "stdafx.h"

enum class err_msg {
	err_open_file,
	err_open_dir,
	err_create_file,
	err_create_dir,
	err_delete_file,
	err_delete_dir,
	err_enum_dir,
	err_file_info,
	err_file_size,
	err_get_ea,
	err_set_ea,
	err_invalid_ea,
	err_set_cs,
	err_get_ft,
	err_set_ft,
	err_get_reparse,
	err_set_reparse,
	err_symlink_length,
	err_hard_link,
	err_read_file,
	err_write_file,
	err_transform_path,
	err_convert_encoding,
	err_archive,
	err_get_version,
	err_version_old,
	err_open_key,
	err_delete_key,
	err_enum_key,
	err_get_key_value,
	err_set_key_value,
	err_delete_key_value,
	err_create_guid,
	err_convert_guid,
	err_distro_not_found,
	err_distro_exists,
	err_distro_running,
	err_no_default_distro,
	err_no_action,
	err_invalid_action,
	err_no_wslapi,
	err_launch_distro,
	err_create_shortcut,
	err_invalid_env,
	err_env_exists,
	err_env_not_found,
	err_config_file,
	err_fs_version,
	err_fs_detect,
	err_root_dir,
	err_invalid_flags,
	err_wsl2_unsupported
};

class lro_error : public std::exception {
	lro_error(const err_msg msg_code, std::vector<wstr> msg_args, HRESULT err_code);
public:
	err_msg msg_code;
	std::vector<wstr> msg_args;
	HRESULT err_code;

	static lro_error from_hresult(err_msg msg_code, std::vector<wstr> msg_args, HRESULT err_code);
	static lro_error from_win32(err_msg msg_code, std::vector<wstr> msg_args, uint32_t err_code);
	static lro_error from_win32_last(err_msg msg_code, std::vector<wstr> msg_args);
	static lro_error from_nt(err_msg msg_code, std::vector<wstr> msg_args, NTSTATUS err_code);
	static lro_error from_other(err_msg msg_code, std::vector<wstr> msg_args);

	wstr format() const;
};
