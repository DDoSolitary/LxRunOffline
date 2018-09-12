#pragma once
#include "stdafx.h"

std::vector<wstr> list_distros();
wstr get_default_distro();
void set_default_distro(crwstr name);
void register_distro(crwstr name, crwstr path);
void unregister_distro(crwstr name);
wstr get_distro_dir(crwstr name);
void set_distro_dir(crwstr name, crwstr value);
uint32_t get_distro_version(crwstr name);
void set_distro_version(crwstr name, uint32_t value);

enum config_mask {
	config_env = 1,
	config_uid = 2,
	config_kernel_cmd = 4,
	config_flags = 8,
	config_all = 15
};

class reg_config {
public:
	std::vector<wstr> env;
	wstr kernel_cmd;
	uint32_t uid, flags;

	reg_config();
	void load_file(crwstr path);
	void save_file(crwstr path) const;
	void load_distro(crwstr name, config_mask desired);
	void configure_distro(crwstr name, config_mask desired) const;
};
