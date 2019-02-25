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

unique_ptr_del<HANDLE> open_file(crwstr path, bool is_dir, bool create) {
	auto h = CreateFile(
		path.c_str(),
		MAXIMUM_ALLOWED, FILE_SHARE_READ, nullptr,
		create ? CREATE_NEW : OPEN_EXISTING,
		is_dir ? FILE_FLAG_BACKUP_SEMANTICS : FILE_FLAG_OPEN_REPARSE_POINT, 0
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
}

template<typename T>
T get_ea(HANDLE hf, const char *name) {
	auto nl = (uint8_t)strlen(name);
	auto gil = (uint32_t)(FIELD_OFFSET(FILE_GET_EA_INFORMATION, EaName) + nl + 1);
	auto bgi = std::make_unique<char[]>(gil);
	auto pgi = (FILE_GET_EA_INFORMATION *)bgi.get();
	auto il = (uint32_t)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + nl + 1 + sizeof(T));
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
	auto il = (uint32_t)(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + nl + 1 + sizeof(T));
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
			set_cs_info(open_file(ap, true, false).get());
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

void time_u2f(const unix_time &ut, LARGE_INTEGER &ft) {
	ft.QuadPart = ut.sec * 10000000 + ut.nsec / 100 + 116444736000000000;
}

void time_f2u(const LARGE_INTEGER &ft, unix_time &ut) {
	auto t = ft.QuadPart - 116444736000000000;
	ut.sec = (uint64_t)(t / 10000000);
	ut.nsec = (uint32_t)(t % 10000000 * 100);
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
	hf_data = open_file(path, false, true);
	write_attr(hf_data.get(), attr);
}

void wsl_writer::write_file_data(const char *buf, uint32_t size) {
	if (size) write_data(hf_data.get(), buf, size);
	else hf_data.reset();
}

void wsl_writer::write_directory(crwstr linux_path, const file_attr &attr) {
	set_path(linux_path);
	if (!CreateDirectory(path.c_str(), nullptr)) {
		auto e = error_win32_last(err_create_dir, { path });
		if (GetLastError() != ERROR_ALREADY_EXISTS) throw e;
		log_warning(format_error(e));
	}
	auto hf = open_file(path, true, false);
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
	auto hf = open_file(path, false, true);
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

wsl_v1_writer::wsl_v1_writer(crwstr base_path)
	: wsl_writer(base_path) {}

void wsl_v1_writer::set_path(crwstr linux_path) {
	path.resize(blen);
	for (auto c : linux_path) {
		if (c == L'/') path += L'\\';
		else if (is_special_char(c)) {
			path += (boost::wformat(L"#%04X") % (uint16_t)c).str();
		} else path += c;
	}
}

void wsl_v1_writer::write_attr(HANDLE hf, const file_attr &attr) {
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

wsl_v2_writer::wsl_v2_writer(crwstr base_path)
	: wsl_writer(base_path) {}

void wsl_v2_writer::real_write_attr(HANDLE hf, const file_attr &attr, crwstr path) const {
	try {
		set_ea(hf, "$LXUID", attr.uid);
		set_ea(hf, "$LXGID", attr.gid);
		set_ea(hf, "$LXMOD", attr.mode);
	} catch (err &e) {
		e.msg_args.push_back(path);
		throw;
	}
	FILE_BASIC_INFO info;
	time_u2f(attr.at, info.LastAccessTime);
	time_u2f(attr.mt, info.LastWriteTime);
	time_u2f(attr.ct, info.ChangeTime);
	info.CreationTime = info.ChangeTime;
	info.FileAttributes = 0;
	if (!SetFileInformationByHandle(hf, FileBasicInfo, &info, sizeof(FILE_BASIC_INFO))) {
		throw error_win32_last(err_set_ft, { path });
	}
}

void wsl_v2_writer::set_path(crwstr linux_path) {
	path.resize(blen);
	for (auto c : linux_path) {
		if (c == L'/') path += L'\\';
		else if (is_special_char(c)) path += c | 0xf000;
		else path += c;
	}
}

void wsl_v2_writer::write_attr(HANDLE hf, const file_attr &attr) {
	if ((attr.mode & AE_IFDIR) == AE_IFDIR) {
		while (!dir_attr.empty()) {
			auto p = dir_attr.top();
			if (!path.compare(0, p.first.size(), p.first)) break;
			dir_attr.pop();
			real_write_attr(open_file(p.first, true, false).get(), p.second, p.first);
		}
		dir_attr.push(std::make_pair(path, attr));
	} else real_write_attr(hf, attr, path);
}

void wsl_v2_writer::write_symlink_data(HANDLE hf, const char *target_path) const {
	auto pl = strlen(target_path);
	auto dl = (uint16_t)(pl + 4);
	auto bl = (uint32_t)(FIELD_OFFSET(REPARSE_DATA_BUFFER, DataBuffer) + dl);
	auto buf = std::make_unique<char[]>(bl);
	auto pb = (REPARSE_DATA_BUFFER *)buf.get();
	pb->ReparseTag = IO_REPARSE_TAG_LX_SYMLINK;
	pb->ReparseDataLength = dl;
	pb->Reserved = 0;
	uint32_t v = 2;
	memcpy(pb->DataBuffer, &v, 4);
	memcpy(pb->DataBuffer + 4, target_path, pl);
	DWORD cnt;
	if (!DeviceIoControl(hf, FSCTL_SET_REPARSE_POINT, pb, bl, nullptr, 0, &cnt, nullptr)) {
		throw error_win32_last(err_set_reparse, { path });
	}
}

wsl_v2_writer::~wsl_v2_writer() {
	while (!dir_attr.empty()) {
		auto p = dir_attr.top();
		dir_attr.pop();
		real_write_attr(open_file(p.first, true, false).get(), p.second, p.first);
	}
}

archive_reader::archive_reader(crwstr archive_path, crwstr root_path)
	: archive_path(archive_path), root_path(root_path) {}

void archive_reader::run(fs_writer &writer) {
	auto rp = root_path;
	append_slash(rp, L'/');
	auto as = get_file_size(open_file(archive_path, false, false).get());
	auto pa = unique_ptr_del<archive *>(archive_read_new(), &archive_read_free);
	check_archive(pa.get(), archive_read_support_filter_all(pa.get()));
	check_archive(pa.get(), archive_read_support_format_all(pa.get()));
	check_archive(pa.get(), archive_read_open_filename_w(pa.get(), archive_path.c_str(), BUFSIZ));
	writer.write_directory(L"", { 0040755,0,0,0,{},{},{} });
	archive_entry *pe;
	while (check_archive(pa.get(), archive_read_next_header(pa.get(), &pe))) {
		print_progress((double)archive_filter_bytes(pa.get(), -1) / as);
		wstr p;
		auto up = archive_entry_pathname(pe);
		if (up) p = from_utf8(up);
		else {
			auto wp = archive_entry_pathname_w(pe);
			if (wp) p = wp;
			else throw error_other(err_convert_encoding, {});
		}
		if (!normalize_linux_path(p, rp)) continue;
		wstr tp;
		bool hl = true;
		auto utp = archive_entry_hardlink(pe);
		if (utp) tp = from_utf8(utp);
		else {
			auto wtp = archive_entry_hardlink_w(pe);
			if (wtp) tp = wtp;
			else hl = false;
		}
		if (hl) {
			if (normalize_linux_path(tp, rp)) {
				writer.write_hard_link(p, tp);
			}
			continue;
		}
		auto type = archive_entry_filetype(pe);
		if (type != AE_IFREG && type != AE_IFDIR && type != AE_IFLNK) {
			log_warning((boost::wformat(L"Ignoring an unsupported file \"%1%\" of type %2$07o.") % p % type).str());
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
			std::unique_ptr<char[]> ptp = nullptr;
			if (!tp) {
				ptp = to_utf8(archive_entry_symlink_w(pe));
				tp = ptp.get();
			}
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
		auto hf = open_file(path, dir, false);
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
		file_attr attr;
		try {
			attr = read_attr(hf.get());
		} catch (const err &e) {
			if (e.msg_code == err_invalid_ea && dir && rp.empty()) {
				attr = { 0040755,0,0,0,{},{},{} };
			} else throw;
		}
		if (dir) writer.write_directory(lp, attr);
		else {
			auto type = attr.mode & AE_IFMT;
			if (type == AE_IFLNK) {
				auto tb = read_symlink_data(hf.get());
				if (tb) writer.write_symlink(lp, attr, tb.get());
				else log_warning((boost::wformat(L"Ignoring an invalid symlink \"%1%\".") % path).str());
				return;
			} else if (type == AE_IFREG) {
				writer.write_new_file(lp, attr);
				DWORD rc;
				do {
					if (!ReadFile(hf.get(), buf, BUFSIZ, &rc, nullptr)) {
						throw error_win32_last(err_read_file, { path });
					}
					writer.write_file_data(buf, rc);
				} while (rc);
			} else log_warning((boost::wformat(L"Ignoring an unsupported file \"%1%\" of type %2$07o.") % path % type).str());
		}
	});
}

wsl_v1_reader::wsl_v1_reader(crwstr base_path)
	: wsl_reader(base_path) {}

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
	buf[sz] = 0;
	return buf;
}

wsl_v2_reader::wsl_v2_reader(crwstr base_path)
	: wsl_reader(base_path) {}

wstr wsl_v2_reader::convert_path(crwstr path) const {
	wstr s;
	for (auto c : path) {
		if (c == L'\\') s += L'/';
		else if ((c & 0xf000) == 0xf000) s += c & ~0xf000;
		else s += c;
	}
	return s;
}

file_attr wsl_v2_reader::read_attr(HANDLE hf) const {
	file_attr attr;
	try {
		attr.uid = get_ea<uint32_t>(hf, "$LXUID");
		attr.gid = get_ea<uint32_t>(hf, "$LXGID");
		attr.mode = get_ea<uint32_t>(hf, "$LXMOD");
		attr.size = get_file_size(hf);
	} catch (err &e) {
		e.msg_args.push_back(path);
		throw;
	}
	FILE_BASIC_INFO info;
	if (!GetFileInformationByHandleEx(hf, FileBasicInfo, &info, sizeof(info))) {
		throw error_win32_last(err_get_ft, { path });
	}
	time_f2u(info.LastAccessTime, attr.at);
	time_f2u(info.LastWriteTime, attr.mt);
	time_f2u(info.ChangeTime, attr.ct);
	return attr;
}

std::unique_ptr<char[]> wsl_v2_reader::read_symlink_data(HANDLE hf) const {
	char buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	DWORD cnt;
	if (!DeviceIoControl(hf, FSCTL_GET_REPARSE_POINT, nullptr, 0, buf, sizeof(buf), &cnt, nullptr)) {
		if (GetLastError() == ERROR_NOT_A_REPARSE_POINT) return nullptr;
		throw error_win32_last(err_get_reparse, { path });
	}
	auto pb = (REPARSE_DATA_BUFFER *)&buf;
	if (pb->ReparseTag != IO_REPARSE_TAG_LX_SYMLINK) return nullptr;
	auto pl = pb->ReparseDataLength - 4;
	auto s = std::make_unique<char[]>(pl + 1);
	memcpy(s.get(), pb->DataBuffer + 4, pl);
	s[pl] = 0;
	return s;
}

uint32_t detect_version(crwstr path) {
	try {
		get_ea<uint32_t>(open_file(normalize_win_path(path) + L"rootfs", true, false).get(), "$LXUID");
	} catch (const err &e) {
		if (e.msg_code == err_invalid_ea) return 1;
		throw;
	}
	return 2;
}

std::unique_ptr<wsl_writer> select_wsl_writer(uint32_t version, crwstr path) {
	if (version <= 1) return std::unique_ptr<wsl_writer>(new wsl_v1_writer(path));
	else if (version == 2) return std::unique_ptr<wsl_writer>(new wsl_v2_writer(path));
	else throw error_other(err_fs_version, { std::to_wstring(version) });
}

std::unique_ptr<wsl_reader> select_wsl_reader(uint32_t version, crwstr path) {
	if (version <= 1) return std::unique_ptr<wsl_reader>(new wsl_v1_reader(path));
	else if (version == 2) return std::unique_ptr<wsl_reader>(new wsl_v2_reader(path));
	else throw error_other(err_fs_version, { std::to_wstring(version) });
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
