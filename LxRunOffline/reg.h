#pragma once
#include "stdafx.h"

std::vector<wstr> list_distros();
wstr get_default_distro();
void set_default_distro(crwstr name);
void register_distro(crwstr name, crwstr path);
void unregister_distro(crwstr name);
wstr get_distro_dir(crwstr name);
void set_distro_dir(crwstr name, crwstr value);
std::vector<wstr> get_distro_env(crwstr name);
void set_distro_env(crwstr name, const std::vector<wstr> &value);
uint32_t get_distro_uid(crwstr name);
void set_distro_uid(crwstr name, uint32_t value);
wstr get_distro_kernel_cmd(crwstr name);
void set_distro_kernel_cmd(crwstr name, crwstr value);
uint32_t get_distro_flags(crwstr name);
void set_distro_flags(crwstr name, uint32_t value);
void duplicate_distro(crwstr name, crwstr new_name, crwstr new_dir);
