#include "stdafx.h"
#include "error.h"
#include "reg.h"
#include "utils.h"

namespace tx = tinyxml2;

const wstr
	reg_base_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss\\",
	vn_default_distro = L"DefaultDistribution",
	vn_distro_name = L"DistributionName",
	vn_dir = L"BasePath",
	vn_state = L"State",
	vn_version = L"Version",
	vn_env = L"DefaultEnvironment",
	vn_uid = L"DefaultUid",
	vn_kernel_cmd = L"KernelCommandLine",
	vn_flags = L"Flags";
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

template<typename T> T get_value(crwstr, crwstr);

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

template<typename T> void set_value(crwstr, crwstr, const T &);

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
	for (crwstr s : value) {
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

unique_ptr_del<HKEY> create_key(crwstr path) {
	HKEY hk;
	auto code = RegCreateKeyEx(
		HKEY_CURRENT_USER, path.c_str(),
		0, nullptr, 0, KEY_READ, nullptr, &hk, nullptr
	);
	if (code) throw error_win32(err_open_key, { path }, code);
	return unique_ptr_del<HKEY>(hk, &RegCloseKey);
}

std::vector<wstr> list_distro_id() {
	std::vector<wstr> res;
	auto hk = create_key(reg_base_path);
	auto ib = std::make_unique<wchar_t[]>(guid_len + 1);
	for (int i = 0;; i++) {
		DWORD bs = guid_len + 1;
		auto code = RegEnumKeyEx(hk.get(), i, ib.get(), &bs, 0, nullptr, nullptr, nullptr);
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
		[](crwstr id) { return  get_value<wstr>(reg_base_path + id, vn_distro_name); }
	);
	return res;
}

wstr get_distro_id(crwstr name) {
	for (crwstr id : list_distro_id()) {
		auto cn = get_value<wstr>(reg_base_path + id, vn_distro_name);
		if (name == cn) return id;
	}
	throw error_other(err_distro_not_found, { name });
}

wstr get_distro_key(crwstr name) {
	return reg_base_path + get_distro_id(name);
}

wstr get_default_distro() {
	try {
		auto p = reg_base_path + get_value<wstr>(reg_base_path, vn_default_distro);
		return get_value<wstr>(p, vn_distro_name);
	} catch (const err &e) {
		if (e.msg_code == err_get_key_value && e.err_code == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			throw error_other(err_no_default_distro, {});
		} else throw;
	}
}

void set_default_distro(crwstr name) {
	set_value(reg_base_path, vn_default_distro, get_distro_id(name));
}

void register_distro(crwstr name, crwstr path, uint32_t version) {
	auto l = list_distros();
	if (count(l.begin(), l.end(), name)) {
		throw error_other(err_distro_exists, { name });
	}

	auto p = reg_base_path + new_guid();
	create_key(p);
	set_value(p, vn_distro_name, name);
	set_value(p, vn_dir, get_full_path(path));
	set_value(p, vn_state, (uint32_t)1);
	set_value(p, vn_version, version);

	try {
		get_default_distro();
	} catch (const err &e) {
		if (e.msg_code == err_no_default_distro) {
			try {
				set_default_distro(name);
			} catch (const err &e) {
				log_warning(format_error(e));
			}
		} else {
			log_warning(format_error(e));
		}
	}
}

void unregister_distro(crwstr name) {
	bool d;
	try {
		d = get_default_distro() == name;
	} catch (const err &e) {
		log_warning(format_error(e));
	}

	auto p = get_distro_key(name);
	auto code = RegDeleteTree(HKEY_CURRENT_USER, p.c_str());
	if (code) throw error_win32(err_delete_key, { p }, code);

	if (d) {
		try {
			auto l = list_distro_id();
			if (l.empty()) {
				auto code = RegDeleteKeyValue(HKEY_CURRENT_USER, reg_base_path.c_str(), vn_default_distro.c_str());
				if (code) throw error_win32(err_delete_key_value, { reg_base_path,vn_default_distro }, code);
			} else {
				set_value(reg_base_path, vn_default_distro, l.front());
			}
		} catch (const err &e) {
			log_warning(format_error(e));
		}
	}
}

wstr get_distro_dir(crwstr name) {
	return get_value<wstr>(get_distro_key(name), vn_dir);
}

void set_distro_dir(crwstr name, crwstr value) {
	set_value(get_distro_key(name), vn_dir, get_full_path(value));
}

uint32_t get_distro_version(crwstr name) {
	return get_value<uint32_t>(get_distro_key(name), vn_version);
}

reg_config::reg_config() {
	env = {
		L"HOSTTYPE=x86_64",
		L"LANG=en_US.UTF-8",
		L"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games",
		L"TERM=xterm-256color"
	};
	uid = 0;
	kernel_cmd = L"BOOT_IMAGE=/kernel init=/init ro";
	flags = 7;
}

err error_xml(tx::XMLError e) {
	return error_other(err_config_file, { from_utf8(tx::XMLDocument::ErrorIDToName(e)) });
}

void reg_config::load_file(crwstr path) {
	auto f = unique_ptr_del<FILE *>(_wfopen(path.c_str(), L"rb"), &fclose);
	if (!f.get()) {
		f.release();
		throw error_win32_last(err_open_file, { path });
	}
	tx::XMLDocument doc;
	auto e = doc.LoadFile(f.get());
	if (e) throw error_xml(e);
	tx::XMLElement *ele, *rt = doc.FirstChildElement("config");
	if (!rt) throw error_other(err_config_file, { L"Root element \"config\" not found." });
	if (ele = rt->FirstChildElement("envs")) {
		env.clear();
		for (auto ee = ele->FirstChildElement("env"); ee; ee = ee->NextSiblingElement("env")) {
			auto s = ee->GetText();
			if (!s) throw error_xml(tx::XML_NO_TEXT_NODE);
			env.push_back(from_utf8(s));
		}
	}
	if (ele = rt->FirstChildElement("uid")) {
		e = ele->QueryUnsignedText(&uid);
		if (e) throw error_xml(e);
	}
	if (ele = rt->FirstChildElement("kernel-cmd")) {
		auto s = ele->GetText();
		if (!s) throw error_xml(tx::XML_NO_TEXT_NODE);
		kernel_cmd = from_utf8(s);
	}
	if (ele = rt->FirstChildElement("flags")) {
		e = ele->QueryUnsignedText(&flags);
		if (e) throw error_xml(e);
	}
}

void reg_config::save_file(crwstr path) const {
	auto f = unique_ptr_del<FILE *>(_wfopen(path.c_str(), L"wb"), &fclose);
	if (!f.get()) {
		f.release();
		throw error_win32_last(err_create_file, { path });
	}
	tx::XMLDocument doc;
	doc.SetBOM(true);
	tx::XMLElement *ele, *rt = doc.NewElement("config");
	doc.InsertEndChild(rt);
	rt->InsertEndChild(ele = doc.NewElement("envs"));
	for (crwstr e : env) {
		tx::XMLElement *ee;
		ele->InsertEndChild(ee = doc.NewElement("env"));
		ee->SetText(to_utf8(e).get());
	}
	rt->InsertEndChild(ele = doc.NewElement("uid"));
	ele->SetText(uid);
	rt->InsertEndChild(ele = doc.NewElement("kernel-cmd"));
	ele->SetText(to_utf8(kernel_cmd).get());
	rt->InsertEndChild(ele = doc.NewElement("flags"));
	ele->SetText(flags);
	auto e = doc.SaveFile(f.get());
	if (e) throw error_xml(e);
}

template<typename T>
void try_get_value(crwstr path, crwstr value_name, T &value) {
	try {
		value = get_value<T>(path, value_name);
	} catch (const err &e) {
		if (e.msg_code != err_get_key_value || e.err_code != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			throw;
		}
	}
}

void reg_config::load_distro(crwstr name, config_mask desired) {
	auto p = get_distro_key(name);
	if (desired & config_env) try_get_value(p, vn_env, env);
	if (desired & config_uid) try_get_value(p, vn_uid, uid);
	if (desired & config_kernel_cmd) try_get_value(p, vn_kernel_cmd, kernel_cmd);
	if (desired & config_flags) try_get_value(p, vn_flags, flags);
}

void reg_config::configure_distro(crwstr name, config_mask desired) const {
	auto p = get_distro_key(name);
	if (desired & config_env) set_value(p, vn_env, env);
	if (desired & config_uid) set_value(p, vn_uid, uid);
	if (desired & config_kernel_cmd) set_value(p, vn_kernel_cmd, kernel_cmd);
	if (desired & config_flags) set_value(p, vn_flags, flags);
}
