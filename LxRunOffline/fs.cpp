#include "stdafx.h"
#include "error.h"
#include "ntdll.h"
#include "utils.h"

struct lxattrb {
	uint16_t flags;
	uint16_t ver;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t rdev;
	uint32_t atime_nsec;
	uint32_t mtime_nsec;
	uint32_t ctime_nsec;
	uint64_t atime;
	uint64_t mtime;
	uint64_t ctime;
};

IO_STATUS_BLOCK iostat;
const char *lx_ea_name = "LXATTRB";
const auto lx_ea_name_len = (uint8_t)strlen(lx_ea_name);
const auto ea_value_offset = lx_ea_name_len + 1;
const auto get_ea_info_len = (uint16_t)(sizeof(FILE_FULL_EA_INFORMATION) + lx_ea_name_len);
const auto ea_info_len = (uint16_t)(sizeof(FILE_FULL_EA_INFORMATION) + lx_ea_name_len + sizeof(lxattrb));

bool check_archive(archive *pa, int stat) {
	if (stat == ARCHIVE_OK) return true;
	if (stat == ARCHIVE_EOF) return false;
	auto es = archive_error_string(pa);
	std::wstringstream ss;
	if (es) ss << es;
	else ss << L"Unknown error " << archive_errno(pa);
	if (stat == ARCHIVE_WARN) {
		log_warning(ss.str());
		return true;
	}
	throw error_other(err_archive, { ss.str() });
}

void set_cs_info(HANDLE dir) {
#ifndef LXRUNOFFLINE_NO_WIN10
	FILE_CASE_SENSITIVE_INFORMATION info;
	auto stat = NtQueryInformationFile(dir, &iostat, &info, sizeof(info), FileCaseSensitiveInformation);
	if (!stat && (info.Flags & FILE_CS_FLAG_CASE_SENSITIVE_DIR)) return;
	info.Flags = FILE_CS_FLAG_CASE_SENSITIVE_DIR;
	stat = NtSetInformationFile(dir, &iostat, &info, sizeof(info), FileCaseSensitiveInformation);
	if (stat) throw error_nt(err_set_cs, {}, stat);
#endif
}

void set_lx_ea(HANDLE file, const lxattrb &data) {
	auto bi = std::make_unique<char[]>(ea_info_len);
	auto info = (FILE_FULL_EA_INFORMATION *)bi.get();
	info->NextEntryOffset = 0;
	info->Flags = 0;
	info->EaNameLength = lx_ea_name_len;
	info->EaValueLength = sizeof(lxattrb);
	strcpy(info->EaName, lx_ea_name);
	memcpy(info->EaName + ea_value_offset, &data, sizeof(lxattrb));
	auto stat = NtSetEaFile(file, &iostat, info, ea_info_len);
	if (stat) throw error_nt(err_set_ea, {}, stat);
}

void copy_lx_ea(HANDLE from_file, HANDLE to_file) {
	auto bgi = std::make_unique<char[]>(get_ea_info_len);
	auto ginfo = (FILE_GET_EA_INFORMATION *)bgi.get();
	auto bi = std::make_unique<char[]>(ea_info_len);
	auto info = (FILE_FULL_EA_INFORMATION *)bi.get();
	ginfo->NextEntryOffset = 0;
	ginfo->EaNameLength = lx_ea_name_len;
	strcpy(ginfo->EaName, lx_ea_name);
	auto stat = NtQueryEaFile(
		from_file, &iostat,
		info, ea_info_len, true,
		ginfo, get_ea_info_len, nullptr, true
	);
	if (stat) throw error_nt(err_get_ea, {}, stat);
	if (info->EaValueLength == sizeof(lxattrb)) {
		stat = NtSetEaFile(to_file, &iostat, info, ea_info_len);
		if (stat) throw error_nt(err_set_ea, {}, stat);
	}
}

unique_val<HANDLE> open_file(crwstr path, bool is_dir, bool create, bool write) {
	if (is_dir && create && !CreateDirectory(path.c_str(), nullptr)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			throw error_win32_last(err_create_dir, { path });
		}
		log_warning((boost::wformat(L"The directory \"%1%\" already exists.") % path).str());
	}
	return unique_val<HANDLE>([&](HANDLE *ph) {
		*ph = CreateFile(
			path.c_str(),
			GENERIC_READ | (write ? GENERIC_WRITE : 0),
			FILE_SHARE_READ, nullptr,
			create && !is_dir ? CREATE_NEW : OPEN_EXISTING,
			is_dir ? FILE_FLAG_BACKUP_SEMANTICS : 0, 0
		);
		if (*ph == INVALID_HANDLE_VALUE) {
			if (is_dir) throw error_win32_last(err_open_dir, { path });
			throw error_win32_last(create ? err_create_file : err_open_file, { path });
		}
	}, &CloseHandle);
}

wstr transform_linux_path(crwstr path, crwstr root) {
	wstr p;
	if (root.empty()) {
		p = path;
	} else if (!path.compare(0, root.size(), root, 0, root.size())) {
		p = path.substr(root.size());
	} else {
		return L"";
	}
	std::wstringstream ss;
	ss << std::setfill(L'0') << std::hex << std::uppercase;
	for (size_t i = 0; i < p.size(); i++) {
		auto c = p[i];
		if (i == 0 && c == '/') continue;
		if (c == L'.' && (i == 0 || p[i - 1] == L'/') && (i == p.size() - 1 || p[i + 1] == '/')) {
			i++;
			continue;
		}
		if (c == L'/') {
			ss << L'\\';
		} else if ((c >= 1 && c <= 31) || c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'\\' || c == L'|' || c == L'*' || c == L'#') {
			ss << L'#' << std::setw(4) << (int)c;
		} else {
			ss << c;
		}
	}
	return ss.str();
}

void append_slash(wstr &path, wchar_t slash) {
	if (!path.empty() && path[path.size() - 1] != slash) {
		path.push_back(slash);
	}
}

wstr get_full_path(crwstr path) {
	auto fp = probe_and_call<wchar_t, int>([&](wchar_t *buf, int len) {
		return GetFullPathName(path.c_str(), len, buf, nullptr);
	});
	if (!fp.second) {
		throw error_win32_last(err_transform_path, { path });
	}
	return fp.first.get();
}

wstr transform_win_path(crwstr path) {
	auto o = L"\\\\?\\" + wstr(get_full_path(path));
	append_slash(o, L'\\');
	return o;
}

void create_directory(crwstr path) {
	for (auto i = path.find(L'\\', 7); i != path.size() - 1; i = path.find(L'\\', i + 1)) {
		auto p = path.substr(0, i);
		if (!CreateDirectory(p.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
			throw error_win32_last(err_create_dir, { p });
		}
	}
}

void extract_archive(crwstr archive_path, crwstr archive_root_path, crwstr target_path) {
	auto rp = archive_root_path;
	append_slash(rp, L'/');
	auto tp = transform_win_path(target_path) + L"rootfs\\";
	create_directory(tp);

	{
		auto ht = open_file(tp, true, true, true);
		try {
			set_cs_info(ht.val);
			set_lx_ea(ht.val, lxattrb{ 0,1,0040755,0,0,0,0,0,0,0,0,0 });
		} catch (err &e) {
			e.push_if_empty(tp);
			throw;
		}
	}

	LARGE_INTEGER as;
	{
		auto ha = open_file(archive_path, false, false, false);
		if (!GetFileSizeEx(ha.val, &as)) as.QuadPart = 0;
	}

	auto pa = unique_val<archive *>(archive_read_new(), &archive_read_free);
	check_archive(pa.val, archive_read_support_filter_all(pa.val));
	check_archive(pa.val, archive_read_support_format_all(pa.val));

	check_archive(pa.val, archive_read_open_filename_w(pa.val, archive_path.c_str(), 65536));

	archive_entry *pe;
	while (check_archive(pa.val, archive_read_next_header(pa.val, &pe))) {
		if (as.QuadPart) print_progress((double)archive_filter_bytes(pa.val, -1) / as.QuadPart);

		auto up = from_utf8(archive_entry_pathname(pe));
		auto frp = transform_linux_path(up, rp);
		if (frp.empty()) continue;
		auto fp = tp + frp;

		auto phl = archive_entry_hardlink(pe);
		if (phl) {
			auto lrp = transform_linux_path(from_utf8(phl), rp);
			if (lrp.empty()) continue;
			auto lp = tp + lrp;
			if (!CreateHardLink(fp.c_str(), lp.c_str(), nullptr)) {
				throw error_win32_last(err_hard_link, { fp,lp });
			}
			continue;
		}
		auto type = archive_entry_filetype(pe);
		if (type != AE_IFREG && type != AE_IFDIR && type != AE_IFLNK) {
			log_warning((boost::wformat(L"Ignoring an unsupported file \"%1%\" of type %2%.") % up % type).str());
			continue;
		}
		try {
			auto hf = open_file(fp, type == AE_IFDIR, true, true);
			auto pst = archive_entry_stat(pe);
			auto mt = (uint64_t)pst->st_mtime;
			auto mtn = (uint32_t)archive_entry_mtime_nsec(pe);
			auto attrb = lxattrb{
				0,1,
				(uint32_t)pst->st_mode,
				(uint32_t)pst->st_uid,
				(uint32_t)pst->st_gid,
				(uint32_t)pst->st_rdev,
				mtn, mtn, mtn,
				mt, mt, mt
			};
			set_lx_ea(hf.val, attrb);

			if (type == AE_IFREG) {
				const void *buf;
				size_t cnt;
				int64_t off;
				DWORD wc;
				while (check_archive(pa.val, archive_read_data_block(pa.val, &buf, &cnt, &off))) {
					if (!WriteFile(hf.val, buf, (uint32_t)cnt, &wc, nullptr)) {
						throw error_win32_last(err_write_file, {});
					}
				}
			} else if (type == AE_IFLNK) {
				auto lp = archive_entry_symlink(pe);
				DWORD wc;
				if (!WriteFile(hf.val, lp, (uint32_t)strlen(lp), &wc, nullptr)) {
					throw error_win32_last(err_write_file, {});
				}
			} else { // AE_IFDIR
				set_cs_info(hf.val);
			}
		} catch (err &e) {
			e.push_if_empty(fp);
			throw;
		}
	}
}

void enum_directory(crwstr root_path, std::function<void(crwstr, int)> action) {
	std::function<void(crwstr)> enum_rec;
	enum_rec = [&](crwstr p) {
		auto ap = root_path + p;
		set_cs_info(open_file(ap, true, false, true).val);
		action(p, 1);
		WIN32_FIND_DATA data;
		auto hs = unique_val<HANDLE>([&](HANDLE *ph) {
			*ph = FindFirstFile((ap + L'*').c_str(), &data);
			if (*ph == INVALID_HANDLE_VALUE) {
				throw error_win32_last(err_enum_dir, { ap });
			}
		}, &FindClose);

		while (true) {
			if (wcscmp(data.cFileName, L".") && wcscmp(data.cFileName, L"..")) {
				auto np = p + data.cFileName;
				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					enum_rec(np + L'\\');
				} else {
					action(np, 0);
				}
			}
			if (!FindNextFile(hs.val, &data)) {
				if (GetLastError() == ERROR_NO_MORE_FILES) {
					action(p, 2);
					return;
				}
				throw error_win32_last(err_enum_dir, { ap });
			}
		}
	};
	enum_rec(L"");
}

void delete_directory(crwstr path) {
	auto dp = transform_win_path(path);
	enum_directory(dp, [&](crwstr p, int f) {
		if (f == 1) return;
		auto del = f ? RemoveDirectory : DeleteFile;
		auto ap = dp + p;
		if (!del((ap.c_str()))) {
			throw error_win32_last(f ? err_delete_dir : err_delete_file, { ap });
		}
	});
}

void copy_directory(crwstr source_path, crwstr target_path) {
	auto sp = transform_win_path(source_path);
	auto tp = transform_win_path(target_path);
	create_directory(tp);
	std::map<uint64_t, wstr> id_map;
	auto buf = std::make_unique<char[]>(BUFSIZ);

	enum_directory(sp, [&](crwstr p, int f) {
		if (f == 2) return;
		auto nsp = sp + p;
		auto ntp = tp + p;
		auto hs = open_file(nsp, f, false, false);

		BY_HANDLE_FILE_INFORMATION info;
		if (!GetFileInformationByHandle(hs.val, &info)) {
			throw error_win32_last(err_file_info, { nsp });
		}

		auto id = info.nFileSizeLow + ((uint64_t)info.nFileSizeHigh << 32);
		if (info.nNumberOfLinks > 1 && id_map.count(id)) {
			if (!CreateHardLink(ntp.c_str(), id_map[id].c_str(), nullptr)) {
				throw error_win32_last(err_hard_link, { ntp,nsp });
			}
		} else {
			if (info.nNumberOfLinks > 1) id_map[id] = ntp;
			auto ht = open_file(ntp, f, true, true);
			try {
				copy_lx_ea(hs.val, ht.val);
			} catch (err &e) {
				e.push_if_empty(e.msg_code == err_get_ea ? nsp : ntp);
				throw;
			}
			if (f) {
				try {
					set_cs_info(ht.val);
				} catch (err &e) {
					e.push_if_empty(ntp);
					throw;
				}
			} else {
				DWORD rc, wc;
				while (true) {
					if (!ReadFile(hs.val, buf.get(), BUFSIZ, &rc, nullptr)) {
						throw error_win32_last(err_read_file, { nsp });
					}
					if (rc == 0) break;
					if (!WriteFile(ht.val, buf.get(), rc, &wc, nullptr)) {
						throw error_win32_last(err_write_file, { ntp });
					}
				}
			}
		}
	});
}

void move_directory(crwstr source_path, crwstr target_path) {
	if (!MoveFile(source_path.c_str(), target_path.c_str())) {
		copy_directory(source_path, target_path);
		delete_directory(source_path);
	}
}
