#pragma once
#include "stdafx.h"

std::vector<wstr> list_distros();
wstr get_default_distro();
void set_default_distro(crwstr name);
void register_distro(crwstr name, crwstr path, uint32_t version);
void unregister_distro(crwstr name);
wstr get_distro_dir(crwstr name);
void set_distro_dir(crwstr name, crwstr value);
uint32_t get_distro_version(crwstr name);

enum config_item_flags {
	config_env = 1,
	config_uid = 2,
	config_kernel_cmd = 4,
	config_flags = 8,
	config_all = 15
};

class reg_config {
	const uint32_t flags_mask = 7, flag_wsl2 = 8;
	uint32_t flags;
public:
	std::vector<wstr> env;
	wstr kernel_cmd;
	uint32_t uid;

	explicit reg_config(bool is_wsl2 = false);
	void load_file(crwstr path);
	void save_file(crwstr path) const;
	void load_distro(crwstr name, config_item_flags desired);
	void configure_distro(crwstr name, config_item_flags desired) const;
	[[nodiscard]] uint32_t get_flags() const;
	void set_flags(uint32_t value);
	[[nodiscard]] bool is_wsl2() const;
};
