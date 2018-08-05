#include "stdafx.h"
#include "error.h"
#include "fs.h"
#include "utils.h"

const wstr
	reg_base_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss\\",
	value_default_distro = L"DefaultDistribution",
	value_distro_name = L"DistributionName",
	value_dir = L"BasePath",
	value_state = L"State",
	value_version = L"Version",
	value_env = L"DefaultEnvironment",
	value_uid = L"DefaultUid",
	value_kernel_cmd = L"KernelCommandLine",
	value_flags = L"Flags",
	default_kernel_cmd = L"BOOT_IMAGE=/kernel init=/init";
const std::vector<wstr> default_env = {
	L"HOSTTYPE=x86_64",
	L"LANG=en_US.UTF-8",
	L"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games",
	L"TERM=xterm-256color"
};
const uint32_t
default_state = 1,
default_version = 1,
default_uid = 0,
default_flags = 7;
const auto guid_len = 38;

wstr new_guid() {
	GUID guid;
	auto hr = CoCreateGuid(&guid);
	if (FAILED(hr)) throw error_hresult(err_create_guid, {}, hr);
	auto buf = std::make_unique<wchar_t[]>(guid_len + 1);
	if (!StringFromGUID2(guid, buf.get(), guid_len + 1)) {
		throw error_other(err_convert_guid, {});
	}
	return buf.get();
}

std::unique_ptr<char[]> get_dynamic(crwstr path, crwstr value_name, uint32_t type) {
	return probe_and_call<char, DWORD>([&](char *buf, DWORD len) {
		auto code = RegGetValue(
			HKEY_CURRENT_USER, path.c_str(),
			value_name.c_str(), type, nullptr, buf, &len
		);
		if (code) throw error_win32(err_get_key_value, { path,value_name }, code);
		return len;
	}).first;
}

void set_dynamic(crwstr path, crwstr value_name, uint32_t type, const void *value, uint32_t len) {
	auto code = RegSetKeyValue(
		HKEY_CURRENT_USER, path.c_str(),
		value_name.c_str(), type, value, len
	);
	if (code) throw error_win32(err_set_key_value, { path,value_name }, code);
}

template<typename T> T get_value(crwstr, crwstr) { assert(false); }

template<>
wstr get_value<wstr>(crwstr path, crwstr value_name) {
	return (wchar_t *)get_dynamic(path, value_name, RRF_RT_REG_SZ).get();
}

template<>
std::vector<wstr> get_value<std::vector<wstr>>(crwstr path, crwstr value_name) {
	std::vector<wstr> v;
	auto buf = get_dynamic(path, value_name, RRF_RT_REG_MULTI_SZ);
	auto ps = (wchar_t *)buf.get();
	while (*ps) {
		v.push_back(ps);
		ps += wcslen(ps) + 1;
	}
	return v;
}

template<>
uint32_t get_value<uint32_t>(crwstr path, crwstr value_name) {
	DWORD res, rlen = sizeof(res);
	auto code = RegGetValue(
		HKEY_CURRENT_USER, path.c_str(),
		value_name.c_str(), RRF_RT_REG_DWORD, nullptr, &res, &rlen
	);
	if (code) throw error_win32(err_get_key_value, { path,value_name }, code);
	return res;
}

template<typename T> void set_value(crwstr, crwstr, const T &) { assert(false); }

template<>
void set_value<wstr>(crwstr path, crwstr value_name, crwstr value) {
	set_dynamic(path, value_name, REG_SZ, value.c_str(), (uint32_t)(value.size() + 1) * sizeof(wchar_t));
}

template<>
void set_value<std::vector<wstr>>(crwstr path, crwstr value_name, const std::vector<wstr> &value) {
	auto cnt = std::accumulate(
		value.begin(), value.end(), 0,
		[](uint32_t cnt, crwstr s) { return cnt + (uint32_t)s.size() + 1; }
	) + 1;
	auto buf = std::make_unique<wchar_t[]>(cnt);
	auto ps = buf.get();
	for (const auto &s : value) {
		std::copy(s.begin(), s.end(), ps);
		ps += s.size();
		*ps = 0;
		ps++;
	}
	*ps = 0;
	set_dynamic(path, value_name, REG_MULTI_SZ, buf.get(), cnt * sizeof(wchar_t));
}

template<>
void set_value<uint32_t>(crwstr path, crwstr value_name, const uint32_t &value) {
	set_dynamic(path, value_name, REG_DWORD, &value, sizeof(value));
}

unique_val<HKEY> create_key(crwstr path, bool write) {
	unique_val<HKEY> hk;
	auto code = RegCreateKeyEx(
		HKEY_CURRENT_USER, path.c_str(),
		0, nullptr, 0, write ? KEY_ALL_ACCESS : KEY_READ, nullptr, &hk.val, nullptr
	);
	if (code) throw error_win32(err_open_key, { path }, code);
	hk.deleter = &RegCloseKey;
	hk.empty = false;
	return hk;
}

std::vector<wstr> list_distro_id() {
	std::vector<wstr> res;
	auto hk = create_key(reg_base_path, false);
	auto ib = std::make_unique<wchar_t[]>(guid_len + 1);
	for (int i = 0;; i++) {
		DWORD bs = guid_len + 1;
		auto code = RegEnumKeyEx(hk.val, i, ib.get(), &bs, 0, nullptr, nullptr, nullptr);
		if (code == ERROR_NO_MORE_ITEMS) break;
		else if (code) throw error_win32(err_enum_key, { reg_base_path }, code);
		res.push_back(ib.get());
	}
	return res;
}

std::vector<wstr> list_distros() {
	auto res = list_distro_id();
	std::transform(
		res.begin(), res.end(), res.begin(),
		[](crwstr id) { return  get_value<wstr>(reg_base_path + id, value_distro_name); }
	);
	return res;
}

wstr get_distro_id(crwstr name) {
	for (const auto &id : list_distro_id()) {
		auto cn = get_value<wstr>(reg_base_path + id, value_distro_name);
		if (name == cn) return id;
	}
	throw error_other(err_distro_not_found, { name });
}

wstr get_distro_key(crwstr name) {
	return reg_base_path + get_distro_id(name);
}

wstr get_default_distro() {
	auto p = reg_base_path + get_value<wstr>(reg_base_path, value_default_distro);
	return get_value<wstr>(p, value_distro_name);
}

void set_default_distro(crwstr name) {
	set_value(reg_base_path, value_default_distro, get_distro_id(name));
}

void register_distro(crwstr name, crwstr path) {
	auto l = list_distros();
	if (count(l.begin(), l.end(), name)) {
		throw error_other(err_distro_exists, { name });
	}

	auto p = reg_base_path + new_guid();
	create_key(p, false);
	set_value(p, value_distro_name, name);
	set_value(p, value_dir, get_full_path(path));
	set_value(p, value_state, default_state);
	set_value(p, value_version, default_version);
}

void unregister_distro(crwstr name) {
	auto p = get_distro_key(name);
	auto code = RegDeleteTree(HKEY_CURRENT_USER, p.c_str());
	if (code) throw error_win32(err_delete_key, { p }, code);
}

wstr get_distro_dir(crwstr name) {
	return get_value<wstr>(get_distro_key(name), value_dir);
}

void set_distro_dir(crwstr name, crwstr value) {
	set_value(get_distro_key(name), value_dir, get_full_path(value));
}

template<typename T>
T get_with_default(crwstr name, crwstr value_name, T default_value) {
	auto p = get_distro_key(name);
	try {
		return get_value<T>(p, value_name);
	} catch (const err &e) {
		if (e.err_code == ERROR_FILE_NOT_FOUND) {
			set_value(p, value_name, default_value);
			return default_value;
		}
		throw;
	}
}

std::vector<wstr> get_distro_env(crwstr name) {
	return get_with_default(name, value_env, default_env);
}

void set_distro_env(crwstr name, const std::vector<wstr> &value) {
	set_value(get_distro_key(name), value_env, value);
}

uint32_t get_distro_uid(crwstr name) {
	return get_with_default(name, value_uid, default_uid);
}

void set_distro_uid(crwstr name, uint32_t value) {
	set_value(get_distro_key(name), value_uid, value);
}

wstr get_distro_kernel_cmd(crwstr name) {
	return get_with_default(name, value_kernel_cmd, default_kernel_cmd);
}

void set_distro_kernel_cmd(crwstr name, crwstr value) {
	set_value(get_distro_key(name), value_kernel_cmd, value);
}

uint32_t get_distro_flags(crwstr name) {
	return get_with_default(name, value_flags, default_flags);
}

void set_distro_flags(crwstr name, uint32_t value) {
	set_value(get_distro_key(name), value_flags, value);
}

void duplicate_distro(crwstr name, crwstr new_name, crwstr new_dir) {
	auto l = list_distros();
	if (count(l.begin(), l.end(), new_name)) {
		throw error_other(err_distro_exists, { new_name });
	}

	auto sp = get_distro_key(name);
	auto tp = reg_base_path + new_guid();
	auto code = RegCopyTree(HKEY_CURRENT_USER, sp.c_str(), create_key(tp, true).val);
	if (code) throw error_win32(err_copy_key, { sp,tp }, code);
	set_value(tp, value_distro_name, new_name);
	set_value(tp, value_dir, get_full_path(new_dir));
}
