#include <boost/test/unit_test.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include "pch.h"
#include "fixtures.h"
#include "utils.h"

using namespace boost::unit_test;

extern "C" const wchar_t *const reg_base_path = fixture_tmp_reg::PATH;

BOOST_AUTO_TEST_SUITE(test_reg)

class reg_key {
	HKEY hk;

	static HKEY dup_hkey(const HKEY hk) {
		const auto hp = GetCurrentProcess();
		HKEY nhk;
		BOOST_TEST_REQUIRE(DuplicateHandle(hp, hk, hp, reinterpret_cast<LPHANDLE>(&nhk), 0, false, DUPLICATE_SAME_ACCESS));
		return nhk;
	}
public:
	explicit reg_key(const HKEY hk) : hk(dup_hkey(hk)) {}

	reg_key(const reg_key &other) : hk(dup_hkey(other.hk)) {}

	~reg_key() {
		RegCloseKey(hk);
	}

	[[nodiscard]]
	reg_key open_subkey(const std::wstring &path, const bool create) const {
		HKEY hk_sub;
		DWORD disp;
		BOOST_TEST_REQUIRE(RegCreateKeyEx(hk, path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hk_sub, &disp) == ERROR_SUCCESS);
		if (create) {
			BOOST_TEST(disp == REG_CREATED_NEW_KEY);
		} else {
			BOOST_TEST(disp == REG_OPENED_EXISTING_KEY);
		}
		const auto ret = reg_key(hk_sub);
		RegCloseKey(hk_sub);
		return ret;
	}

	[[nodiscard]]
	std::vector<std::wstring> get_subkey_names() const {
		DWORD subkey_count, subkey_max_len;
		BOOST_TEST_REQUIRE(RegQueryInfoKey(hk, nullptr, nullptr, nullptr, &subkey_count, &subkey_max_len, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
		std::vector<std::wstring> ret;
		const auto buf = std::make_unique<wchar_t[]>(static_cast<size_t>(subkey_max_len) + 1);
		for (DWORD i = 0; i < subkey_count; i++) {
			auto len = subkey_max_len + 1;
			BOOST_TEST_REQUIRE(RegEnumKeyEx(hk, i, buf.get(), &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
			ret.emplace_back(buf.get());
		}
		return ret;
	}

	[[nodiscard]]
	DWORD get_value_count() const {
		DWORD ret;
		BOOST_TEST_REQUIRE(RegQueryInfoKey(hk, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &ret, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
		return ret;
	}

	[[nodiscard]]
	DWORD get_dword_value(const std::wstring &name) const {
		DWORD ret, len = sizeof(DWORD);
		BOOST_TEST_REQUIRE(RegGetValue(hk, nullptr, name.c_str(), RRF_RT_DWORD, nullptr, &ret, &len) == ERROR_SUCCESS);
		BOOST_TEST(len == sizeof(DWORD));
		return ret;
	}

	void set_dword_value(const std::wstring &name, const DWORD value) const {
		BOOST_TEST_REQUIRE(RegSetKeyValue(hk, nullptr, name.c_str(), REG_DWORD, &value, sizeof(DWORD)) == ERROR_SUCCESS);
	}

	[[nodiscard]]
	std::wstring get_sz_value(const std::wstring &name) const {
		DWORD len = 0;
		BOOST_TEST_REQUIRE(RegGetValue(hk, nullptr, name.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &len) == ERROR_SUCCESS);
		const auto buf = std::make_unique<wchar_t[]>(static_cast<size_t>(len));
		BOOST_TEST_REQUIRE(RegGetValue(hk, nullptr, name.c_str(), RRF_RT_REG_SZ, nullptr, buf.get(), &len) == ERROR_SUCCESS);
		return buf.get();
	}

	void set_sz_value(const std::wstring &name, const std::wstring &value) const {
		const auto len = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
		BOOST_TEST_REQUIRE(RegSetKeyValue(hk, nullptr, name.c_str(), REG_SZ, value.c_str(), len) == ERROR_SUCCESS);
	}

	[[nodiscard]]
	std::vector<std::wstring> get_multi_sz_value(const std::wstring &name) const {
		DWORD len = 0;
		BOOST_TEST_REQUIRE(RegGetValue(hk, nullptr, name.c_str(), RRF_RT_REG_MULTI_SZ, nullptr, nullptr, &len) == ERROR_SUCCESS);
		const auto buf = std::make_unique<wchar_t[]>(static_cast<size_t>(len));
		BOOST_TEST_REQUIRE(RegGetValue(hk, nullptr, name.c_str(), RRF_RT_REG_MULTI_SZ, nullptr, buf.get(), &len) == ERROR_SUCCESS);
		std::vector<std::wstring> ret;
		for (const auto *p = buf.get(); (p - buf.get() + 1) * 2 < len;) {
			const auto str = std::wstring(p);
			p += str.size() + 1;
			ret.push_back(str);
		}
		return ret;
	}

	void set_multi_sz_value(const std::wstring &name, const std::vector<std::wstring> &value) const {
		std::vector<wchar_t> buf;
		for (const auto &str : value) {
			buf.insert(buf.end(), str.begin(), str.end());
			buf.push_back(0);
		}
		buf.push_back(0);
		const auto len = static_cast<DWORD>(buf.size() * sizeof(wchar_t));
		BOOST_TEST_REQUIRE(RegSetKeyValue(hk, nullptr, name.c_str(), REG_MULTI_SZ, buf.data(), len) == ERROR_SUCCESS);
	}
};

static std::wstring register_test_distro(const reg_key &root_key, const std::wstring &name, const std::wstring &path) {
	auto distro_id = new_guid();
	const auto distro_key = root_key.open_subkey(distro_id, true);
	distro_key.set_sz_value(L"DistributionName", name);
	distro_key.set_sz_value(L"BasePath", path);
	distro_key.set_dword_value(L"State", 1);
	distro_key.set_dword_value(L"Version", 2);
	return distro_id;
}

BOOST_FIXTURE_TEST_CASE(test_list_distros, fixture_tmp_reg) {
	const auto root_key = reg_key(get_hkey());
	register_test_distro(root_key, L"foo1", L"C:\\bar1");
	register_test_distro(root_key, L"foo2", L"C:\\bar2");
	register_test_distro(root_key, L"foo3", L"C:\\bar3");
	const auto non_guid_key = root_key.open_subkey(L"AppxInstallerCache", true);
	const auto long_key = root_key.open_subkey(
		L"a string which is longer than the buffer for GUID in list_distro_id", true);
	auto list = list_distros();
	std::sort(list.begin(), list.end());
	BOOST_TEST(list == (std::vector<std::wstring> { L"foo1", L"foo2", L"foo3" }));
}

BOOST_FIXTURE_TEST_CASE(test_register_distro, fixture_tmp_reg) {
	register_distro(L"foo", L"C:\\bar1", 2);
	const auto root_key = reg_key(get_hkey());
	const auto subkeys = root_key.get_subkey_names();
	BOOST_TEST(subkeys.size() == 1);
	const auto distro_key = root_key.open_subkey(subkeys[0], false);
	BOOST_TEST(distro_key.get_value_count() == 4);
	BOOST_TEST(distro_key.get_sz_value(L"DistributionName").c_str() == L"foo");
	BOOST_TEST(distro_key.get_sz_value(L"BasePath").c_str() == L"C:\\bar1");
	BOOST_TEST(distro_key.get_dword_value(L"State") == 1);
	BOOST_TEST(distro_key.get_dword_value(L"Version") == 2);
	BOOST_CHECK_THROW(register_distro(L"foo", L"C:\\bar2", 2), lro_error);
}

BOOST_FIXTURE_TEST_CASE(test_unregister_distro, fixture_tmp_reg) {
	const auto root_key = reg_key(get_hkey());
	const auto distro_id = register_test_distro(root_key, L"foo", L"C:\\bar");
	BOOST_CHECK_THROW(unregister_distro(L"foo"), lro_error);
	root_key.set_sz_value(L"DefaultDistribution", distro_id);
	BOOST_CHECK_THROW(unregister_distro(L"bar"), lro_error);
	unregister_distro(L"foo");
	BOOST_TEST(root_key.get_subkey_names().empty());
}

BOOST_FIXTURE_TEST_CASE(test_get_distro_dir, fixture_tmp_reg) {
	BOOST_CHECK_THROW(get_distro_dir(L"foo"), lro_error);
	const auto root_key = reg_key(get_hkey());
	register_test_distro(root_key, L"foo", L"C:\\bar");
	BOOST_TEST(_wcsicmp(get_distro_dir(L"foo").c_str(), L"C:\\bar") == 0);
}

BOOST_FIXTURE_TEST_CASE(test_set_distro_dir, fixture_tmp_reg) {
	BOOST_CHECK_THROW(set_distro_dir(L"foo", L"C:\\bar"), lro_error);
	const auto root_key = reg_key(get_hkey());
	const auto distro_id = register_test_distro(root_key, L"foo", L"");
	const auto distro_key = root_key.open_subkey(distro_id, false);
	set_distro_dir(L"foo", L"C:\\bar");
	BOOST_TEST(_wcsicmp(distro_key.get_sz_value(L"BasePath").c_str(), L"C:\\bar") == 0);
	set_distro_dir(L"foo", L"bar");
	BOOST_TEST(_wcsicmp(distro_key.get_sz_value(L"BasePath").c_str(), (std::filesystem::current_path() / L"bar").wstring().c_str()) == 0);
}

BOOST_FIXTURE_TEST_CASE(test_get_distro_version, fixture_tmp_reg) {
	BOOST_CHECK_THROW(get_distro_version(L"foo"), lro_error);
	const auto root_key = reg_key(get_hkey());
	register_test_distro(root_key, L"foo", L"C:\\bar");
	BOOST_TEST(get_distro_version(L"foo") == 2);
}

BOOST_AUTO_TEST_SUITE(test_default_distro)

BOOST_FIXTURE_TEST_CASE(test_get_default_distro, fixture_tmp_reg) {
	BOOST_CHECK_THROW(get_default_distro(), lro_error);
	const auto root_key = reg_key(get_hkey());
	const auto distro_id = register_test_distro(root_key, L"foo", L"C:\\bar");
	root_key.set_sz_value(L"DefaultDistribution", distro_id);
	BOOST_TEST(get_default_distro().c_str() == L"foo");
}

BOOST_FIXTURE_TEST_CASE(test_set_default_distro, fixture_tmp_reg) {
	BOOST_CHECK_THROW(set_default_distro(L"foo"), lro_error);
	const auto root_key = reg_key(get_hkey());
	const auto distro_id = register_test_distro(root_key, L"foo", L"C:\\bar");
	set_default_distro(L"foo");
	BOOST_TEST(root_key.get_sz_value(L"DefaultDistribution").c_str() == distro_id.c_str());
}

BOOST_FIXTURE_TEST_CASE(test_register_default, fixture_tmp_reg) {
	register_distro(L"foo", L"C:\\bar", 2);
	const auto root_key = reg_key(get_hkey());
	BOOST_TEST(root_key.get_value_count() == 1);
	const auto default_distro = root_key.get_sz_value(L"DefaultDistribution");
	const auto distro_id = root_key.get_subkey_names()[0];
	BOOST_TEST(default_distro.c_str() == distro_id.c_str());
}

BOOST_FIXTURE_TEST_CASE(test_register_non_default, fixture_tmp_reg) {
	const auto root_key = reg_key(get_hkey());
	const auto distro_id = register_test_distro(root_key, L"foo1", L"C:\\bar1");
	root_key.set_sz_value(L"DefaultDistribution", distro_id);
	register_distro(L"foo2", L"C:\\bar2", 2);
	BOOST_TEST(root_key.get_sz_value(L"DefaultDistribution").c_str() == distro_id.c_str());
}

BOOST_FIXTURE_TEST_CASE(test_unregister_default, fixture_tmp_reg) {
	const auto root_key = reg_key(get_hkey());
	const auto distro_id = register_test_distro(root_key, L"foo", L"C:\\bar");
	root_key.set_sz_value(L"DefaultDistribution", distro_id);
	unregister_distro(L"foo");
	BOOST_TEST(root_key.get_value_count() == 0);
}

BOOST_FIXTURE_TEST_CASE(test_unregister_non_default, fixture_tmp_reg) {
	const auto root_key = reg_key(get_hkey());
	const auto distro_id = register_test_distro(root_key, L"foo1", L"C:\\bar1");
	root_key.set_sz_value(L"DefaultDistribution", distro_id);
	register_test_distro(root_key, L"foo2", L"C:\\bar2");
	unregister_distro(L"foo2");
	BOOST_TEST(root_key.get_sz_value(L"DefaultDistribution").c_str() == distro_id.c_str());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(test_reg_config)

BOOST_AUTO_TEST_CASE(test_default_value) {
	reg_config conf(false);
	const std::vector<std::wstring> expected_env = {
		L"HOSTTYPE=x86_64",
		L"LANG=en_US.UTF-8",
		L"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games",
		L"TERM=xterm-256color"
	};
	BOOST_TEST(conf.env == expected_env);
	BOOST_TEST(conf.uid == 0);
	BOOST_TEST(conf.kernel_cmd.c_str() == L"BOOT_IMAGE=/kernel init=/init ro");
	BOOST_TEST(conf.get_flags() == 7);
}

static reg_config get_test_conf(const bool is_wsl2) {
	reg_config conf(is_wsl2);
	conf.env = { L"FOO1=bar1", L"FOO2=bar2" };
	conf.uid = 42;
	conf.kernel_cmd = L"foo";
	conf.set_flags(1);
	return conf;
}

BOOST_TEST_DECORATOR(*fixture<fixture_tmp_dir>())
BOOST_AUTO_TEST_CASE(test_load_save_file) {
	const auto conf1 = get_test_conf(true);
	reg_config conf2(false);
	conf1.save_file(L"foo.xml");
	BOOST_CHECK_THROW(conf2.load_file(L"bar.xml"), lro_error);
	conf2.load_file(L"foo.xml");
	BOOST_TEST(conf1.env == conf2.env);
	BOOST_TEST(conf1.uid == conf2.uid);
	BOOST_TEST(conf1.kernel_cmd.c_str() == conf2.kernel_cmd.c_str());
	BOOST_TEST(conf1.get_flags() == conf2.get_flags());
}

BOOST_DATA_TEST_CASE_F(fixture_tmp_reg, test_load_mask, data::xrange(0, 8), mask) {
	const auto root_key = reg_key(get_hkey());
	const auto distro_key = root_key.open_subkey(register_test_distro(root_key, L"foo", L"C:\\bar"), false);
	const auto conf1 = get_test_conf(false);
	if (mask & config_env) {
		distro_key.set_multi_sz_value(L"DefaultEnvironment", conf1.env);
	}
	if (mask & config_uid) {
		distro_key.set_dword_value(L"DefaultUid", conf1.uid);
	}
	if (mask & config_kernel_cmd) {
		distro_key.set_sz_value(L"KernelCommandLine", conf1.kernel_cmd);
	}
	if (mask & config_flags) {
		distro_key.set_dword_value(L"Flags", conf1.get_flags());
	}
	const reg_config conf2(false);
	auto conf3 = conf2;
	conf3.load_distro(L"foo", static_cast<config_item_flags>(mask));
	if (mask & config_env) {
		BOOST_TEST(conf3.env == conf1.env);
	} else {
		BOOST_TEST(conf3.env == conf2.env);
	}
	if (mask & config_uid) {
		BOOST_TEST(conf3.uid == conf1.uid);
	} else {
		BOOST_TEST(conf3.uid == conf2.uid);
	}
	if (mask & config_kernel_cmd) {
		BOOST_TEST(conf3.kernel_cmd.c_str() == conf1.kernel_cmd.c_str());
	} else {
		BOOST_TEST(conf3.kernel_cmd.c_str() == conf2.kernel_cmd.c_str());
	}
	if (mask & config_flags) {
		BOOST_TEST(conf3.get_flags() == conf1.get_flags());
	} else {
		BOOST_TEST(conf3.get_flags() == conf2.get_flags());
	}
}

BOOST_FIXTURE_TEST_CASE(test_load_default, fixture_tmp_reg) {
	BOOST_CHECK_THROW(reg_config(false).load_distro(L"foo", config_all), lro_error);
	const auto root_key = reg_key(get_hkey());
	register_test_distro(root_key, L"foo", L"C:\\bar");
	const auto conf1 = get_test_conf(false);
	auto conf2 = conf1;
	conf2.load_distro(L"foo", config_all);
	BOOST_TEST(conf1.env == conf2.env);
	BOOST_TEST(conf1.uid == conf2.uid);
	BOOST_TEST(conf1.kernel_cmd.c_str() == conf2.kernel_cmd.c_str());
	BOOST_TEST(conf1.get_flags() == conf2.get_flags());
}

BOOST_DATA_TEST_CASE_F(fixture_tmp_reg, test_configure_mask, data::xrange(0, 8), mask) {
	BOOST_CHECK_THROW(reg_config(false).configure_distro(L"foo", static_cast<config_item_flags>(mask)), lro_error);
	const auto root_key = reg_key(get_hkey());
	const auto distro_key = root_key.open_subkey(register_test_distro(root_key, L"foo", L"C:\\bar"), false);
	const auto old_value_count = distro_key.get_value_count();
	const auto conf = get_test_conf(false);
	conf.configure_distro(L"foo", static_cast<config_item_flags>(mask));
	auto config_count = 0;
	if (mask & config_env) {
		config_count++;
		BOOST_TEST(distro_key.get_multi_sz_value(L"DefaultEnvironment") == conf.env);
	}
	if (mask & config_uid) {
		config_count++;
		BOOST_TEST(distro_key.get_dword_value(L"DefaultUid") == conf.uid);
	}
	if (mask & config_kernel_cmd) {
		config_count++;
		BOOST_TEST(distro_key.get_sz_value(L"KernelCommandLine").c_str() == conf.kernel_cmd.c_str());
	}
	if (mask & config_flags) {
		config_count++;
		BOOST_TEST(distro_key.get_dword_value(L"Flags") == conf.get_flags());
	}
	BOOST_TEST(distro_key.get_value_count() == old_value_count + config_count);
}

BOOST_TEST_DECORATOR(*fixture<fixture_tmp_dir>())
BOOST_FIXTURE_TEST_CASE(test_wsl2_flag, fixture_tmp_reg) {
	auto conf1 = get_test_conf(false);
	BOOST_TEST(!conf1.is_wsl2());
	BOOST_CHECK_THROW(conf1.set_flags(conf1.get_flags() | 8), lro_error);
	conf1 = get_test_conf(true);
	BOOST_TEST((conf1.get_flags() & 8) == 0);
	BOOST_TEST(conf1.is_wsl2());
	conf1.set_flags(0);
	BOOST_TEST(conf1.is_wsl2());
	const auto root_key = reg_key(get_hkey());
	const auto distro_key = root_key.open_subkey(register_test_distro(root_key, L"foo", L"C:\\bar"), false);
	conf1.configure_distro(L"foo", config_flags);
	BOOST_TEST(distro_key.get_dword_value(L"Flags") == (conf1.get_flags() | 8));
	reg_config conf2(false);
	conf2.load_distro(L"foo", config_flags);
	BOOST_TEST(conf2.get_flags() == conf1.get_flags());
	BOOST_TEST(conf2.is_wsl2());
	conf1.save_file(L"foo.xml");
	conf2 = reg_config(false);
	conf2.load_file(L"foo.xml");
	BOOST_TEST(conf2.get_flags() == conf1.get_flags());
	BOOST_TEST(conf2.is_wsl2());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
