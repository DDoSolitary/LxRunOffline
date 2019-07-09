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

unique_ptr_del<HANDLE> open_file(crwstr path, bool is_dir, bool create, bool no_share = false) {
	auto h = CreateFile(
		path.c_str(),
		MAXIMUM_ALLOWED, no_share ? 0 : FILE_SHARE_READ, nullptr,
		create ? CREATE_NEW : OPEN_EXISTING,
		is_dir ? FILE_FLAG_BACKUP_SEMANTICS : FILE_FLAG_OPEN_REPARSE_POINT, 0
	);
	if (h == INVALID_HANDLE_VALUE) {
		if (is_dir) throw error_win32_last(err_open_dir, { path });
		throw error_win32_last(create ? err_create_file : err_open_file, { path });
	}
	return unique_ptr_del<HANDLE>(h, &CloseHandle);
}

void create_recursive(crwstr path) {
	for (auto i = path.find(L'\\', 7); i != wstr::npos; i = path.find(L'\\', i + 1)) {
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
	unique_ptr_del<PSECURITY_DESCRIPTOR> pd(pdb, &LocalFree);
	EXPLICIT_ACCESS ea;
	BuildExplicitAccessWithName(&ea, (wchar_t *)L"CURRENT_USER", FILE_DELETE_CHILD, GRANT_ACCESS, CONTAINER_INHERIT_ACE);
	PACL pnab;
	if (SetEntriesInAcl(1, &ea, pa, &pnab)) return;
	unique_ptr_del<PACL> pna(pnab, &LocalFree);
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

void enum_directory(file_path &path, bool rootfs_first, std::function<void(enum_dir_type)> action) {
	std::function<void(bool)> enum_rec;
	enum_rec = [&](bool is_root) {
		try {
			set_cs_info(open_file(path.data, true, false).get());
		} catch (err &e) {
			if (e.msg_code == err_set_cs) e.msg_args.push_back(path.data);
			throw;
		}
		action(enum_dir_enter);
		auto os = path.data.size();
		if (is_root) {
			path.data += L"rootfs\\";
			enum_rec(false);
			path.data.resize(os);
		}
		WIN32_FIND_DATA data;
		path.data += L'*';
		unique_ptr_del<HANDLE> hs(FindFirstFile(path.data.c_str(), &data), &FindClose);
		path.data.resize(os);
		if (hs.get() == INVALID_HANDLE_VALUE) {
			hs.release();
			throw error_win32_last(err_enum_dir, { path.data });
		}
		while (true) {
			if (wcscmp(data.cFileName, L".") && wcscmp(data.cFileName, L"..") && (!is_root || wcscmp(data.cFileName, L"rootfs"))) {
				path.data += data.cFileName;
				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					path.data += L'\\';
					enum_rec(false);
				} else {
					action(enum_dir_file);
				}
				path.data.resize(os);
			}
			if (!FindNextFile(hs.get(), &data)) {
				if (GetLastError() == ERROR_NO_MORE_FILES) {
					action(enum_dir_exit);
					return;
				}
				throw error_win32_last(err_enum_dir, { path.data });
			}
		}
	};
	enum_rec(rootfs_first);
}

void time_u2f(const unix_time &ut, LARGE_INTEGER &ft) {
	ft.QuadPart = ut.sec * 10000000 + ut.nsec / 100 + 116444736000000000;
}

void time_f2u(const LARGE_INTEGER &ft, unix_time &ut) {
	auto t = ft.QuadPart - 116444736000000000;
	ut.sec = (uint64_t)(t / 10000000);
	ut.nsec = (uint32_t)(t % 10000000 * 100);
}

archive_writer::archive_writer(crwstr archive_path)
	: pa(archive_write_new(), &archive_write_free), pe(archive_entry_new(), &archive_entry_free), ignored_files() {
	path = std::make_unique<linux_path>();
	target_path = std::make_unique<linux_path>();
	check_archive(pa.get(), archive_write_set_format_gnutar(pa.get()));
	check_archive(pa.get(), archive_write_add_filter_gzip(pa.get()));
	check_archive(pa.get(), archive_write_open_filename_w(pa.get(), archive_path.c_str()));
}

void archive_writer::write_entry(const file_attr &attr) {
	auto up = to_utf8(path->data);
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

bool archive_writer::check_attr(const file_attr *attr) {
	if (!attr) {
		warn_ignored(path->data);
		ignored_files.insert(path->data);
		return false;
	}
	return true;
}

void archive_writer::warn_ignored(crwstr path) {
	log_warning((boost::wformat(L"Ignoring the file \"%1%\" which doesn't have WSL attributes.") % path).str());
}

bool archive_writer::write_new_file(const file_attr *attr) {
	if (!check_attr(attr)) return false;
	write_entry(*attr);
	return true;
}

void archive_writer::write_file_data(const char *buf, uint32_t size) {
	if (size) {
		if (archive_write_data(pa.get(), buf, size) < 0) {
			check_archive(pa.get(), ARCHIVE_FATAL);
		}
	}
}

void archive_writer::write_directory(const file_attr *attr) {
	if (!check_attr(attr)) return;
	if (!path->data.empty()) write_entry(*attr);
}

void archive_writer::write_symlink(const file_attr *attr, const char *target) {
	if (!check_attr(attr)) return;
	archive_entry_set_symlink(pe.get(), target);
	write_entry(*attr);
}

void archive_writer::write_hard_link() {
	if (ignored_files.find(target_path->data) != ignored_files.end()) {
		warn_ignored(path->data);
		return;
	}
	auto up = to_utf8(path->data);
	archive_entry_set_pathname(pe.get(), up.get());
	auto ut = to_utf8(target_path->data);
	archive_entry_set_hardlink(pe.get(), ut.get());
	check_archive(pa.get(), archive_write_header(pa.get(), pe.get()));
	archive_entry_clear(pe.get());
}

wsl_writer::wsl_writer() : hf_data(nullptr) {}

void wsl_writer::write_data(HANDLE hf, const char *buf, uint32_t size) const {
	DWORD wc;
	if (!WriteFile(hf, buf, size, &wc, nullptr)) {
		throw error_win32_last(err_write_file, { path->data });
	}
}

bool wsl_writer::write_new_file(const file_attr *attr) {
	hf_data = open_file(path->data, false, true);
	write_attr(hf_data.get(), attr);
	return true;
}

void wsl_writer::write_file_data(const char *buf, uint32_t size) {
	if (size) write_data(hf_data.get(), buf, size);
	else hf_data.reset();
}

void wsl_writer::write_directory(const file_attr *attr) {
	if (!CreateDirectory(path->data.c_str(), nullptr)) {
		auto e = error_win32_last(err_create_dir, { path->data });
		if (GetLastError() != ERROR_ALREADY_EXISTS) throw e;
		log_warning(format_error(e));
	}
	auto hf = open_file(path->data, true, false);
	write_attr(hf.get(), attr);
	try {
		set_cs_info(hf.get());
	} catch (err &e) {
		e.msg_args.push_back(path->data);
		throw;
	}
}

void wsl_writer::write_symlink(const file_attr *attr, const char *target) {
	auto hf = open_file(path->data, false, true);
	write_attr(hf.get(), attr);
	write_symlink_data(hf.get(), target);
}

void wsl_writer::write_hard_link() {
	if (!CreateHardLink(path->data.c_str(), target_path->data.c_str(), nullptr)) {
		throw error_win32_last(err_hard_link, { path->data, target_path->data });
	}
}

wsl_v1_writer::wsl_v1_writer(crwstr base_path) {
	path = std::make_unique<wsl_v1_path>(base_path);
	target_path = std::make_unique<wsl_v1_path>(base_path);
	create_recursive(path->data);
}

void wsl_v1_writer::write_attr(HANDLE hf, const file_attr *attr) {
	if (!attr) return;
	try {
		set_ea(hf, "LXATTRB", lxattrb {
			0, 1,
			attr->mode, attr->uid, attr->gid,
			0,
			attr->at.nsec, attr->mt.nsec, attr->ct.nsec,
			attr->at.sec, attr->mt.sec, attr->ct.sec
		});
	} catch (err &e) {
		e.msg_args.push_back(path->data);
		throw;
	}
}

void wsl_v1_writer::write_symlink_data(HANDLE hf, const char *target) const {
	write_data(hf, target, (uint32_t)strlen(target));
}

wsl_v2_writer::wsl_v2_writer(crwstr base_path) {
	path = std::make_unique<wsl_v2_path>(base_path);
	target_path = std::make_unique<wsl_v2_path>(base_path);
	create_recursive(path->data);
}

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

void wsl_v2_writer::write_attr(HANDLE hf, const file_attr *attr) {
	if (!attr) return;
	if ((attr->mode & AE_IFDIR) == AE_IFDIR) {
		while (!dir_attr.empty()) {
			auto p = dir_attr.top();
			if (!path->data.compare(0, p.first.size(), p.first)) break;
			dir_attr.pop();
			real_write_attr(open_file(p.first, true, false).get(), p.second, p.first);
		}
		dir_attr.push(std::make_pair(path->data, *attr));
	} else real_write_attr(hf, *attr, path->data);
}

void wsl_v2_writer::write_symlink_data(HANDLE hf, const char *target) const {
	auto pl = strlen(target);
	auto dl = (uint16_t)(pl + 4);
	auto bl = (uint32_t)(FIELD_OFFSET(REPARSE_DATA_BUFFER, DataBuffer) + dl);
	auto buf = std::make_unique<char[]>(bl);
	auto pb = (REPARSE_DATA_BUFFER *)buf.get();
	pb->ReparseTag = IO_REPARSE_TAG_LX_SYMLINK;
	pb->ReparseDataLength = dl;
	pb->Reserved = 0;
	uint32_t v = 2;
	memcpy(pb->DataBuffer, &v, 4);
	memcpy(pb->DataBuffer + 4, target, pl);
	DWORD cnt;
	if (!DeviceIoControl(hf, FSCTL_SET_REPARSE_POINT, pb, bl, nullptr, 0, &cnt, nullptr)) {
		throw error_win32_last(err_set_reparse, { path->data });
	}
}

wsl_v2_writer::~wsl_v2_writer() {
	while (!dir_attr.empty()) {
		auto p = dir_attr.top();
		dir_attr.pop();
		real_write_attr(open_file(p.first, true, false).get(), p.second, p.first);
	}
}

wsl_legacy_writer::wsl_legacy_writer(crwstr base_path) {
	path = std::make_unique<wsl_legacy_path>(base_path);
	target_path = std::make_unique<wsl_legacy_path>(base_path);
	create_recursive(path->data);
}

archive_reader::archive_reader(crwstr archive_path, crwstr root_path)
	: archive_path(archive_path), root_path(root_path) {}

void archive_reader::run(fs_writer &writer) {
	auto as = get_file_size(open_file(archive_path, false, false).get());
	unique_ptr_del<archive *> pa(archive_read_new(), &archive_read_free);
	check_archive(pa.get(), archive_read_support_filter_all(pa.get()));
	check_archive(pa.get(), archive_read_support_format_all(pa.get()));
	check_archive(pa.get(), archive_read_open_filename_w(pa.get(), archive_path.c_str(), BUFSIZ));
	linux_path p;
	if (p.convert(*writer.path)) {
		file_attr attr { 0040755, 0, 0, 0, {}, {}, {} };
		writer.write_directory(&attr);
	}
	archive_entry *pe;
	while (check_archive(pa.get(), archive_read_next_header(pa.get(), &pe))) {
		print_progress((double)archive_filter_bytes(pa.get(), -1) / as);
		auto up = archive_entry_pathname(pe);
		auto wp = archive_entry_pathname_w(pe);
		if (up) p = linux_path(from_utf8(up), root_path);
		else if (wp) p = linux_path(wp, root_path);
		else throw error_other(err_convert_encoding, {});
		if (!p.convert(*writer.path)) continue;
		auto utp = archive_entry_hardlink(pe);
		auto wtp = archive_entry_hardlink_w(pe);
		if (utp || wtp) {
			linux_path tp;
			if (utp) tp = linux_path(from_utf8(utp), root_path);
			else tp = linux_path(wtp, root_path);
			if (tp.convert(*writer.target_path)) writer.write_hard_link();
			continue;
		}
		auto type = archive_entry_filetype(pe);
		if (type != AE_IFREG && type != AE_IFDIR && type != AE_IFLNK) {
			log_warning((boost::wformat(L"Ignoring an unsupported file \"%1%\" of type %2$07o.") % p.data % type).str());
			continue;
		}
		auto pst = archive_entry_stat(pe);
		unix_time mt {
			(uint64_t)pst->st_mtime,
			(uint32_t)archive_entry_mtime_nsec(pe)
		};
		file_attr attr {
			(uint32_t)pst->st_mode, (uint32_t)pst->st_uid, (uint32_t)pst->st_gid, (uint64_t)pst->st_size,
			archive_entry_atime_is_set(pe) ? unix_time { (uint64_t)pst->st_atime, (uint32_t)archive_entry_atime_nsec(pe) } : mt,
			mt,
			archive_entry_ctime_is_set(pe) ? unix_time { (uint64_t)pst->st_ctime, (uint32_t)archive_entry_ctime_nsec(pe) } : mt
		};
		if (type == AE_IFREG) {
			if (!writer.write_new_file(&attr)) continue;
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
			writer.write_symlink(&attr, tp);
		} else { // AE_IFDIR
			writer.write_directory(&attr);
		}
	}
}

bool wsl_reader::is_legacy() const {
	return false;
}

void wsl_reader::run(fs_writer &writer) {
	std::map<uint64_t, std::unique_ptr<file_path>> id_map;
	char buf[BUFSIZ];
	bool is_root = true;
	enum_directory(*path, is_legacy(), [&](enum_dir_type t) {
		if (t == enum_dir_exit) return;
		if (t == enum_dir_enter && is_root) {
			is_root = false;
			return;
		}
		if (!path->convert(*writer.path)) return;
		bool dir = t == enum_dir_enter;
		auto hf = open_file(path->data, dir, false);
		if (!dir) {
			BY_HANDLE_FILE_INFORMATION info;
			if (!GetFileInformationByHandle(hf.get(), &info)) {
				throw error_win32_last(err_file_info, { path->data });
			}
			if (info.nNumberOfLinks > 1) {
				auto id = info.nFileIndexLow + ((uint64_t)info.nFileIndexHigh << 32);
				if (id_map.count(id)) {
					if (id_map[id]->convert(*writer.target_path)) writer.write_hard_link();
					return;
				} else id_map[id] = path->clone();
			}
		}
		auto attr = read_attr(hf.get());
		if (dir) writer.write_directory(attr.get());
		else {
			auto type = attr ? attr->mode & AE_IFMT : AE_IFREG;
			if (type == AE_IFLNK) {
				auto tb = read_symlink_data(hf.get());
				if (tb) writer.write_symlink(attr.get(), tb.get());
				else log_warning((boost::wformat(L"Ignoring an invalid symlink \"%1%\".") % path->data).str());
				return;
			} else if (type == AE_IFREG) {
				if (!writer.write_new_file(attr.get())) return;
				DWORD rc;
				do {
					if (!ReadFile(hf.get(), buf, BUFSIZ, &rc, nullptr)) {
						throw error_win32_last(err_read_file, { path->data });
					}
					writer.write_file_data(buf, rc);
				} while (rc);
			} else log_warning((boost::wformat(L"Ignoring an unsupported file \"%1%\" of type %2$07o.") % path->data % type).str());
		}
	});
}

wsl_v1_reader::wsl_v1_reader(crwstr base) {
	path = std::make_unique<wsl_v1_path>(base);
}

std::unique_ptr<file_attr> wsl_v1_reader::read_attr(HANDLE hf) const {
	try {
		auto ea = get_ea<lxattrb>(hf, "LXATTRB");
		return std::unique_ptr<file_attr>(new file_attr {
			ea.mode, ea.uid, ea.gid, get_file_size(hf),
			{ ea.atime, ea.atime_nsec },
			{ ea.mtime, ea.mtime_nsec },
			{ ea.ctime, ea.ctime_nsec }
		});
	} catch (err &e) {
		if (e.msg_code == err_invalid_ea) return nullptr;
		e.msg_args.push_back(path->data);
		throw;
	}
}

std::unique_ptr<char[]> wsl_v1_reader::read_symlink_data(HANDLE hf) const {
	uint64_t sz;
	try {
		sz = get_file_size(hf);
	} catch (err &e) {
		e.msg_args.push_back(path->data);
	}
	if (sz > 65536) throw error_other(err_symlink_length, { path->data, std::to_wstring(sz) });
	auto buf = std::make_unique<char[]>(sz + 1);
	DWORD rc;
	for (uint32_t off = 0; off < sz; off += rc) {
		if (!ReadFile(hf, buf.get() + off, (uint32_t)(sz - off), &rc, nullptr)) {
			throw error_win32_last(err_read_file, { path->data });
		}
	}
	buf[sz] = 0;
	return buf;
}

wsl_v2_reader::wsl_v2_reader(crwstr base) {
	path = std::make_unique<wsl_v2_path>(base);
}

std::unique_ptr<file_attr> wsl_v2_reader::read_attr(HANDLE hf) const {
	std::unique_ptr<file_attr> attr(new file_attr);
	try {
		attr->uid = get_ea<uint32_t>(hf, "$LXUID");
		attr->gid = get_ea<uint32_t>(hf, "$LXGID");
		attr->mode = get_ea<uint32_t>(hf, "$LXMOD");
		attr->size = get_file_size(hf);
	} catch (err &e) {
		if (e.msg_code == err_invalid_ea) return nullptr;
		e.msg_args.push_back(path->data);
		throw;
	}
	FILE_BASIC_INFO info;
	if (!GetFileInformationByHandleEx(hf, FileBasicInfo, &info, sizeof(info))) {
		throw error_win32_last(err_get_ft, { path->data });
	}
	time_f2u(info.LastAccessTime, attr->at);
	time_f2u(info.LastWriteTime, attr->mt);
	time_f2u(info.ChangeTime, attr->ct);
	return attr;
}

std::unique_ptr<char[]> wsl_v2_reader::read_symlink_data(HANDLE hf) const {
	char buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	DWORD cnt;
	if (!DeviceIoControl(hf, FSCTL_GET_REPARSE_POINT, nullptr, 0, buf, sizeof(buf), &cnt, nullptr)) {
		if (GetLastError() == ERROR_NOT_A_REPARSE_POINT) return nullptr;
		throw error_win32_last(err_get_reparse, { path->data });
	}
	auto pb = (REPARSE_DATA_BUFFER *)&buf;
	if (pb->ReparseTag != IO_REPARSE_TAG_LX_SYMLINK) return nullptr;
	auto pl = pb->ReparseDataLength - 4;
	auto s = std::make_unique<char[]>(pl + 1);
	memcpy(s.get(), pb->DataBuffer + 4, pl);
	s[pl] = 0;
	return s;
}

bool wsl_legacy_reader::is_legacy() const {
	return true;
}

wsl_legacy_reader::wsl_legacy_reader(crwstr base) {
	path = std::make_unique<wsl_legacy_path>(base);
}

template<typename T>
bool has_ea(crwstr path, const char *name, bool ignore_error) {
	try {
		get_ea<T>(open_file(path, true, false).get(), name);
		return true;
	} catch (const err &e) {
		if (e.msg_code == err_invalid_ea || ignore_error) return false;
		throw;
	}
}

uint32_t detect_version(crwstr path) {
	wsl_v2_path p1(path), p2(path);
	p1.data += L"rootfs\\";
	p2.data += L"home\\";
	if (has_ea<uint32_t>(p1.data, "$LXUID", false)) return 2;
	if (has_ea<lxattrb>(p2.data, "LXATTRB", true)) return 0;
	if (has_ea<lxattrb>(p1.data, "LXATTRB", false)) return 1;
	throw error_other(err_fs_detect, { path });
}

bool detect_wsl2(crwstr path) {
	wsl_v2_path p(path);
	try {
		open_file(p.data + L"ext4.vhdx", false, false);
		return true;
	} catch (const err &e) {
		if (e.msg_code == err_open_file && e.err_code == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return false;
		throw;
	}
}

std::unique_ptr<wsl_writer> select_wsl_writer(uint32_t version, crwstr path) {
	if (version == 0) return std::make_unique<wsl_legacy_writer>(path);
	else if (version == 1) return std::make_unique<wsl_v1_writer>(path);
	else if (version == 2) return std::make_unique<wsl_v2_writer>(path);
	else throw error_other(err_fs_version, { std::to_wstring(version) });
}

std::unique_ptr<wsl_reader> select_wsl_reader(uint32_t version, crwstr path) {
	if (version == 0) return std::make_unique<wsl_legacy_reader>(path);
	else if (version == 1) return std::make_unique<wsl_v1_reader>(path);
	else if (version == 2) return std::make_unique<wsl_v2_reader>(path);
	else throw error_other(err_fs_version, { std::to_wstring(version) });
}

bool move_directory(crwstr source_path, crwstr target_path) {
	return MoveFile(source_path.c_str(), target_path.c_str());
}

void delete_directory(crwstr path) {
	wsl_v2_path p(path);
	enum_directory(p, false, [&](enum_dir_type t) {
		if (t == enum_dir_enter) return;
		bool dir = t == enum_dir_exit;
		if (!(dir ? RemoveDirectory : DeleteFile)(p.data.c_str())) {
			throw error_win32_last(dir ? err_delete_dir : err_delete_file, { p.data });
		}
	});
}

bool check_in_use(crwstr path) {
	try {
		open_file(path, false, false, true);
	} catch (const err &e) {
		if (e.msg_code == err_open_file && e.err_code == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)) {
			return true;
		}
	}
	return false;
}
