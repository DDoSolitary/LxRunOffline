#include "stdafx.h"
#include "error.h"
#include "fs.h"
#include "ntdll.h"
#include "utils.h"

enum enum_dir_type {
	enum_dir_enter,
	enum_dir_exit,
	enum_dir_file
};

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

unique_ptr_del<HANDLE> open_file(crwstr path, bool is_dir, bool create, bool write) {
	auto h = CreateFile(
		path.c_str(),
		GENERIC_READ | (write ? GENERIC_WRITE : 0),
		FILE_SHARE_READ, nullptr,
		create ? CREATE_NEW : OPEN_EXISTING,
		is_dir ? FILE_FLAG_BACKUP_SEMANTICS : 0, 0
	);
	if (h == INVALID_HANDLE_VALUE) {
		if (is_dir) throw error_win32_last(err_open_dir, { path });
		throw error_win32_last(create ? err_create_file : err_open_file, { path });
	}
	return unique_ptr_del<HANDLE>(h, &CloseHandle);
}

void create_parents(crwstr path) {
	for (auto i = path.find(L'\\', 7); i != path.size() - 1; i = path.find(L'\\', i + 1)) {
		auto p = path.substr(0, i);
		if (!CreateDirectory(p.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
			throw error_win32_last(err_create_dir, { p });
		}
	}
}

uint64_t get_file_size(HANDLE hf) {
	LARGE_INTEGER sz;
	if (!GetFileSizeEx(hf, &sz)) throw error_win32_last(err_file_size, {});
	return sz.QuadPart;
}

void grant_delete_child(HANDLE hf) {
	PACL pa;
	PSECURITY_DESCRIPTOR pdb;
	if (GetSecurityInfo(hf, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pa, nullptr, &pdb)) return;
	auto pd = unique_ptr_del<PSECURITY_DESCRIPTOR>(pdb, &LocalFree);
	EXPLICIT_ACCESS ea;
	BuildExplicitAccessWithName(&ea, (wchar_t *)L"CURRENT_USER", FILE_DELETE_CHILD, GRANT_ACCESS, CONTAINER_INHERIT_ACE);
	PACL pnab;
	if (SetEntriesInAcl(1, &ea, pa, &pnab)) return;
	auto pna = unique_ptr_del<PACL>(pnab, &LocalFree);
	SetSecurityInfo(hf, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pna.get(), nullptr);
}

void set_cs_info(HANDLE hd) {
#ifndef LXRUNOFFLINE_NO_WIN10
	FILE_CASE_SENSITIVE_INFORMATION info;
	auto stat = NtQueryInformationFile(hd, &iostat, &info, sizeof(info), FileCaseSensitiveInformation);
	if (!stat && (info.Flags & FILE_CS_FLAG_CASE_SENSITIVE_DIR)) return;
	info.Flags = FILE_CS_FLAG_CASE_SENSITIVE_DIR;
	stat = NtSetInformationFile(hd, &iostat, &info, sizeof(info), FileCaseSensitiveInformation);
	if (stat == STATUS_ACCESS_DENIED) {
		grant_delete_child(hd);
		stat = NtSetInformationFile(hd, &iostat, &info, sizeof(info), FileCaseSensitiveInformation);
	}
	if (stat) throw error_nt(err_set_cs, {}, stat);
#endif
}

template<typename T>
T get_ea(HANDLE hf, const char *name) {
	auto nl = (uint8_t)strlen(name);
	auto gil = (uint32_t)(sizeof(FILE_GET_EA_INFORMATION) + nl);
	auto bgi = std::make_unique<char[]>(gil);
	auto pgi = (FILE_GET_EA_INFORMATION *)bgi.get();
	auto il = (uint32_t)(sizeof(FILE_FULL_EA_INFORMATION) + nl + sizeof(T));
	auto bi = std::make_unique<char[]>(il);
	auto pi = (FILE_FULL_EA_INFORMATION *)bi.get();
	pgi->NextEntryOffset = 0;
	pgi->EaNameLength = nl;
	strcpy(pgi->EaName, name);
	auto stat = NtQueryEaFile(
		hf, &iostat,
		pi, il, true,
		pgi, gil, nullptr, true
	);
	if (stat) throw error_nt(err_get_ea, {}, stat);
	if (pi->EaValueLength != sizeof(T)) {
		throw error_other(err_invalid_ea, { from_utf8(name) });
	}
	T t;
	memcpy(&t, pi->EaName + nl + 1, sizeof(T));
	return t;
}

template<typename T>
void set_ea(HANDLE hf, const char *name, const T &data) {
	auto nl = (uint8_t)strlen(name);
	auto il = (uint32_t)(sizeof(FILE_FULL_EA_INFORMATION) + nl + sizeof(T));
	auto bi = std::make_unique<char[]>(il);
	auto pi = (FILE_FULL_EA_INFORMATION *)bi.get();
	pi->NextEntryOffset = 0;
	pi->Flags = 0;
	pi->EaNameLength = nl;
	pi->EaValueLength = sizeof(T);
	strcpy(pi->EaName, name);
	memcpy(pi->EaName + nl + 1, &data, sizeof(T));
	auto stat = NtSetEaFile(hf, &iostat, pi, il);
	if (stat) throw error_nt(err_set_ea, { from_utf8(name) }, stat);
}

void enum_directory(crwstr root_path, std::function<void(crwstr, enum_dir_type)> action) {
	std::function<void(crwstr)> enum_rec;
	enum_rec = [&](crwstr p) {
		auto ap = root_path + p;
		try {
			set_cs_info(open_file(ap, true, false, true).get());
		} catch (err &e) {
			if (e.msg_code == err_set_cs) e.msg_args.push_back(ap);
			throw;
		}
		action(p, enum_dir_enter);
		WIN32_FIND_DATA data;
		auto hs = unique_ptr_del<HANDLE>(FindFirstFile((ap + L'*').c_str(), &data), &FindClose);
		if (hs.get() == INVALID_HANDLE_VALUE) {
			hs.release();
			throw error_win32_last(err_enum_dir, { ap });
		}
		while (true) {
			if (wcscmp(data.cFileName, L".") && wcscmp(data.cFileName, L"..")) {
				auto np = p + data.cFileName;
				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					enum_rec(np + L'\\');
				} else {
					action(np, enum_dir_file);
				}
			}
			if (!FindNextFile(hs.get(), &data)) {
				if (GetLastError() == ERROR_NO_MORE_FILES) {
					action(p, enum_dir_exit);
					return;
				}
				throw error_win32_last(err_enum_dir, { ap });
			}
		}
	};
	enum_rec(L"");
}

bool is_special_char(wchar_t c) {
	return (c >= 1 && c <= 31) || c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'\\' || c == L'|' || c == L'*' || c == L'#';
}

void append_slash(wstr &path, wchar_t slash) {
	if (!path.empty() && path.back() != slash) {
		path += slash;
	}
}

wstr normalize_win_path(crwstr path) {
	auto o = L"\\\\?\\" + get_full_path(path);
	append_slash(o, L'\\');
	return o;
}

bool normalize_linux_path(wstr &path, crwstr root) {
	size_t rp = 0, wp = 0;
	if (!root.empty()) {
		if (!path.compare(0, root.size(), root)) rp += root.size();
		else return false;
	}
	bool cb = true;
	while (rp < path.size()) {
		if (cb) {
			if (path[rp] == L'/') {
				rp++;
				continue;
			}
			if (!path.compare(rp, 2, L"./")) {
				rp += 2;
				continue;
			}
			if (!path.compare(rp, 3, L"../")) {
				rp += 3;
				if (wp) {
					auto sp = path.rfind(L'/', wp - 2);
					if (sp == wstr::npos) wp = 0;
					else wp = sp + 1;
				}
				continue;
			}
		}
		cb = path[rp] == L'/';
		path[wp++] = path[rp++];
	}
	path.resize(wp);
	return wp > 0;
}

archive_writer::archive_writer(crwstr path)
	: pa(archive_write_new(), &archive_write_free), pe(archive_entry_new(), &archive_entry_free) {
	check_archive(pa.get(), archive_write_set_format_gnutar(pa.get()));
	check_archive(pa.get(), archive_write_add_filter_gzip(pa.get()));
	check_archive(pa.get(), archive_write_open_filename_w(pa.get(), path.c_str()));
}

void archive_writer::write_entry(crwstr path, const file_attr &attr) {
	auto up = to_utf8(path);
	archive_entry_set_pathname(pe.get(), up.get());
	archive_entry_set_uid(pe.get(), attr.uid);
	archive_entry_set_gid(pe.get(), attr.gid);
	archive_entry_set_mode(pe.get(), attr.mode);
	archive_entry_set_size(pe.get(), attr.size);
	archive_entry_set_atime(pe.get(), attr.at.sec, attr.at.nsec);
	archive_entry_set_mtime(pe.get(), attr.mt.sec, attr.mt.nsec);
	archive_entry_set_ctime(pe.get(), attr.ct.sec, attr.ct.nsec);
	check_archive(pa.get(), archive_write_header(pa.get(), pe.get()));
	archive_entry_clear(pe.get());
}

void archive_writer::write_new_file(crwstr path, const file_attr &attr) {
	write_entry(path, attr);
}

void archive_writer::write_file_data(const char *buf, uint32_t size) {
	if (size) {
		if (archive_write_data(pa.get(), buf, size) < 0) {
			check_archive(pa.get(), ARCHIVE_FATAL);
		}
	}
}

void archive_writer::write_directory(crwstr path, const file_attr &attr) {
	if (!path.empty()) write_entry(path, attr);
}

void archive_writer::write_symlink(crwstr path, const file_attr &attr, const char *target_path) {
	archive_entry_set_symlink(pe.get(), target_path);
	write_entry(path, attr);
}

void archive_writer::write_hard_link(crwstr path, crwstr target_path) {
	auto up = to_utf8(path);
	archive_entry_set_pathname(pe.get(), up.get());
	auto ut = to_utf8(target_path);
	archive_entry_set_hardlink(pe.get(), ut.get());
	check_archive(pa.get(), archive_write_header(pa.get(), pe.get()));
	archive_entry_clear(pe.get());
}

wsl_writer::wsl_writer(crwstr base_path)
	: path(normalize_win_path(base_path) + L"rootfs\\"), blen(path.size()), hf_data(nullptr) {
	create_parents(path);
}

void wsl_writer::write_data(HANDLE hf, const char *buf, uint32_t size) const {
	DWORD wc;
	if (!WriteFile(hf, buf, size, &wc, nullptr)) {
		throw error_win32_last(err_write_file, { path });
	}
}

void wsl_writer::write_new_file(crwstr linux_path, const file_attr &attr) {
	set_path(linux_path);
	hf_data = open_file(path, false, true, true);
	write_attr(hf_data.get(), attr);
}

void wsl_writer::write_file_data(const char *buf, uint32_t size) {
	if (size) write_data(hf_data.get(), buf, size);
	else hf_data.reset(nullptr);
}

void wsl_writer::write_directory(crwstr linux_path, const file_attr &attr) {
	set_path(linux_path);
	if (!CreateDirectory(path.c_str(), nullptr)) {
		auto e = error_win32_last(err_create_dir, { path });
		if (GetLastError() != ERROR_ALREADY_EXISTS) throw e;
		log_warning(format_error(e));
	}
	auto hf = open_file(path, true, false, true);
	write_attr(hf.get(), attr);
	try {
		set_cs_info(hf.get());
	} catch (err &e) {
		e.msg_args.push_back(path);
		throw;
	}
}

void wsl_writer::write_symlink(crwstr linux_path, const file_attr &attr, const char *target_path) {
	set_path(linux_path);
	auto hf = open_file(path, false, true, true);
	write_attr(hf.get(), attr);
	write_symlink_data(hf.get(), target_path);
}

void wsl_writer::write_hard_link(crwstr linux_path, crwstr target_linux_path) {
	set_path(target_linux_path);
	auto tp = path;
	set_path(linux_path);
	if (!CreateHardLink(path.c_str(), tp.c_str(), nullptr)) {
		throw error_win32_last(err_hard_link, { path,tp });
	}
}

void wsl_v1_writer::set_path(crwstr linux_path) {
	path.resize(blen);
	for (auto c : linux_path) {
		if (c == L'/') path += L'\\';
		else if (is_special_char(c)) {
			path += (boost::wformat(L"#%04X") % (uint16_t)c).str();
		} else path += c;
	}
}

void wsl_v1_writer::write_attr(HANDLE hf, const file_attr &attr) const {
	try {
		set_ea(hf, "LXATTRB", lxattrb{
			0,1,
			attr.mode,attr.uid,attr.gid,
			0,
			attr.at.nsec,attr.mt.nsec,attr.ct.nsec,
			attr.at.sec,attr.mt.sec,attr.ct.sec
		});
	} catch (err &e) {
		e.msg_args.push_back(path);
		throw;
	}
}

void wsl_v1_writer::write_symlink_data(HANDLE hf, const char *target_path) const {
	write_data(hf, target_path, (uint32_t)strlen(target_path));
}

archive_reader::archive_reader(crwstr archive_path, crwstr root_path)
	: archive_path(archive_path), root_path(root_path) {}

void archive_reader::run(fs_writer &writer) {
	auto rp = root_path;
	append_slash(rp, L'/');
	auto as = get_file_size(open_file(archive_path, false, false, false).get());
	auto pa = unique_ptr_del<archive *>(archive_read_new(), &archive_read_free);
	check_archive(pa.get(), archive_read_support_filter_all(pa.get()));
	check_archive(pa.get(), archive_read_support_format_all(pa.get()));
	check_archive(pa.get(), archive_read_open_filename_w(pa.get(), archive_path.c_str(), BUFSIZ));
	writer.write_directory(L"", { 0040755,0,0,0,{},{},{} });
	archive_entry *pe;
	while (check_archive(pa.get(), archive_read_next_header(pa.get(), &pe))) {
		print_progress((double)archive_filter_bytes(pa.get(), -1) / as);
		auto p = from_utf8(archive_entry_pathname(pe));
		if (!normalize_linux_path(p, rp)) continue;
		auto utp = archive_entry_hardlink(pe);
		if (utp) {
			auto tp = from_utf8(utp);
			if (normalize_linux_path(tp, rp)) {
				writer.write_hard_link(p, tp);
			}
			continue;
		}
		auto type = archive_entry_filetype(pe);
		if (type != AE_IFREG && type != AE_IFDIR && type != AE_IFLNK) {
			log_warning((boost::wformat(L"Ignoring an unsupported file \"%1%\" of type %2%.") % p % type).str());
			continue;
		}
		auto pst = archive_entry_stat(pe);
		auto mt = unix_time{
			(uint64_t)pst->st_mtime,
			(uint32_t)archive_entry_mtime_nsec(pe)
		};
		auto attr = file_attr{
			(uint32_t)pst->st_mode,(uint32_t)pst->st_uid,(uint32_t)pst->st_gid,(uint64_t)pst->st_size,
			archive_entry_atime_is_set(pe) ? unix_time{ (uint64_t)pst->st_atime,(uint32_t)archive_entry_atime_nsec(pe) } : mt,
			mt,
			archive_entry_ctime_is_set(pe) ? unix_time{ (uint64_t)pst->st_ctime,(uint32_t)archive_entry_ctime_nsec(pe) } : mt
		};
		if (type == AE_IFREG) {
			writer.write_new_file(p, attr);
			const void *buf;
			size_t cnt;
			int64_t off;
			while (check_archive(pa.get(), archive_read_data_block(pa.get(), &buf, &cnt, &off))) {
				writer.write_file_data((const char *)buf, (uint32_t)cnt);
			}
			writer.write_file_data(nullptr, 0);
		} else if (type == AE_IFLNK) {
			auto tp = archive_entry_symlink(pe);
			writer.write_symlink(p, attr, tp);
		} else { // AE_IFDIR
			writer.write_directory(p, attr);
		}
	}
}

wsl_reader::wsl_reader(crwstr base_path)
	: path(normalize_win_path(base_path) + L"rootfs\\"), blen(path.size()) {}

void wsl_reader::run(fs_writer &writer) {
	std::map<uint64_t, wstr> id_map;
	char buf[BUFSIZ];
	enum_directory(wstr(path), [&](crwstr rp, enum_dir_type t) {
		if (t == enum_dir_exit) return;
		bool dir = t == enum_dir_enter;
		path.resize(blen);
		path += rp;
		auto lp = convert_path(rp);
		auto hf = open_file(path, dir, false, false);
		if (!dir) {
			BY_HANDLE_FILE_INFORMATION info;
			if (!GetFileInformationByHandle(hf.get(), &info)) {
				throw error_win32_last(err_file_info, { path });
			}
			if (info.nNumberOfLinks > 1) {
				auto id = info.nFileIndexLow + ((uint64_t)info.nFileIndexHigh << 32);
				if (id_map.count(id)) {
					writer.write_hard_link(lp, id_map[id]);
					return;
				} else id_map[id] = lp;
			}
		}
		auto attr = read_attr(hf.get());
		if (dir) writer.write_directory(lp, attr);
		else {
			if ((attr.mode & AE_IFLNK) == AE_IFLNK) {
				auto tb = read_symlink_data(hf.get());
				if (tb) {
					writer.write_symlink(lp, attr, tb.get());
					return;
				}
			}
			writer.write_new_file(lp, attr);
			DWORD rc;
			do {
				if (!ReadFile(hf.get(), buf, BUFSIZ, &rc, nullptr)) {
					throw error_win32_last(err_read_file, { path });
				}
				writer.write_file_data(buf, rc);
			} while (rc);
		}
	});
}

wstr wsl_v1_reader::convert_path(crwstr path) const {
	wstr s;
	for (size_t i = 0; i < path.size(); i++) {
		if (path[i] == L'\\') s += L'/';
		else if (path[i] == L'#') {
			s += (wchar_t)stoi(path.substr(i + 1, 4), nullptr, 16);
			i += 4;
		} else s += path[i];
	}
	return s;
}

file_attr wsl_v1_reader::read_attr(HANDLE hf) const {
	try {
		auto ea = get_ea<lxattrb>(hf, "LXATTRB");
		return {
			ea.mode,ea.uid,ea.gid,get_file_size(hf),
			{ ea.atime,ea.atime_nsec },
			{ ea.mtime,ea.mtime_nsec },
			{ ea.ctime,ea.ctime_nsec }
		};
	} catch (err &e) {
		e.msg_args.push_back(path);
		throw;
	}
}

std::unique_ptr<char[]> wsl_v1_reader::read_symlink_data(HANDLE hf) const {
	uint64_t sz;
	try {
		sz = get_file_size(hf);
	} catch (err &e) {
		e.msg_args.push_back(path);
	}
	if (sz > 65536) throw error_other(err_symlink_length, { path,std::to_wstring(sz) });
	auto buf = std::make_unique<char[]>(sz + 1);
	DWORD rc;
	for (uint32_t off = 0; off < sz; off += rc) {
		if (!ReadFile(hf, buf.get() + off, (uint32_t)(sz - off), &rc, nullptr)) {
			throw error_win32_last(err_read_file, { path });
		}
	}
	buf.get()[sz] = 0;
	return buf;
}

bool move_directory(crwstr source_path, crwstr target_path) {
	return MoveFile(source_path.c_str(), target_path.c_str());
}

void delete_directory(crwstr path) {
	auto dp = normalize_win_path(path);
	enum_directory(dp, [&](crwstr p, enum_dir_type t) {
		if (t == enum_dir_enter) return;
		bool dir = t == enum_dir_exit;
		auto del = dir ? RemoveDirectory : DeleteFile;
		auto ap = dp + p;
		if (!del((ap.c_str()))) {
			throw error_win32_last(dir ? err_delete_dir : err_delete_file, { ap });
		}
	});
}
