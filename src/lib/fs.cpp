#include "pch.h"
#include "error.h"
#include "fs.h"
#include "ntdll.h"
#include "utils.h"

enum class enum_dir_type {
	enter,
	exit,
	file
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

bool check_archive(archive *pa, const int stat) {
	if (stat == ARCHIVE_OK) return true;
	if (stat == ARCHIVE_EOF) return false;
	const auto es = archive_error_string(pa);
	std::wstringstream ss;
	if (es) ss << es;
	else ss << L"Unknown error " << archive_errno(pa);
	if (stat == ARCHIVE_WARN) {
		log_warning(ss.str());
		return true;
	}
	throw lro_error::from_other(err_msg::err_archive, { ss.str() });
}

unique_ptr_del<HANDLE> open_file(crwstr path, const bool is_dir, const bool create, const bool no_share = false) {
	const auto h = CreateFile(
		path.c_str(),
		MAXIMUM_ALLOWED, no_share ? 0 : FILE_SHARE_READ, nullptr,
		create ? CREATE_NEW : OPEN_EXISTING,
		is_dir ? FILE_FLAG_BACKUP_SEMANTICS : FILE_FLAG_OPEN_REPARSE_POINT, nullptr
	);
	if (h == INVALID_HANDLE_VALUE) {
		if (is_dir) throw lro_error::from_win32_last(err_msg::err_open_dir, { path });
		throw lro_error::from_win32_last(create ? err_msg::err_create_file : err_msg::err_open_file, { path });
	}
	return unique_ptr_del<HANDLE>(h, &CloseHandle);
}

void create_recursive(crwstr path) {
	for (auto i = path.find(L'\\', 7); i != wstr::npos; i = path.find(L'\\', i + 1)) {
		auto p = path.substr(0, i);
		if (!CreateDirectory(p.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
			throw lro_error::from_win32_last(err_msg::err_create_dir, { p });
		}
	}
}

uint64_t get_file_size(const HANDLE hf) {
	LARGE_INTEGER sz;
	if (!GetFileSizeEx(hf, &sz)) throw lro_error::from_win32_last(err_msg::err_file_size, {});
	return sz.QuadPart;
}

void grant_delete_child(const HANDLE hf) {
	PACL pa;
	PSECURITY_DESCRIPTOR pdb;
	if (GetSecurityInfo(hf, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pa, nullptr, &pdb)) return;
	unique_ptr_del<PSECURITY_DESCRIPTOR> pd(pdb, &LocalFree);
	EXPLICIT_ACCESS ea;
	BuildExplicitAccessWithName(
		&ea, const_cast<wchar_t *>(L"CURRENT_USER"),
		FILE_DELETE_CHILD, GRANT_ACCESS, CONTAINER_INHERIT_ACE
	);
	PACL pnab;
	if (SetEntriesInAcl(1, &ea, pa, &pnab)) return;
	const unique_ptr_del<PACL> pna(pnab, &LocalFree);
	SetSecurityInfo(hf, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pna.get(), nullptr);
}

void set_cs_info(const HANDLE hd) {
	FILE_CASE_SENSITIVE_INFORMATION info = {};
	auto stat = NtQueryInformationFile(hd, &iostat, &info, sizeof info, FileCaseSensitiveInformation);
	if (!stat && (info.Flags & FILE_CS_FLAG_CASE_SENSITIVE_DIR)) return;
	info.Flags = FILE_CS_FLAG_CASE_SENSITIVE_DIR;
	stat = NtSetInformationFile(hd, &iostat, &info, sizeof info, FileCaseSensitiveInformation);
	if (stat == STATUS_ACCESS_DENIED) {
		grant_delete_child(hd);
		stat = NtSetInformationFile(hd, &iostat, &info, sizeof info, FileCaseSensitiveInformation);
	}
	if (stat) throw lro_error::from_nt(err_msg::err_set_cs, {}, stat);
}

template<typename T>
T get_ea(const HANDLE hf, const char *name) {
	const auto nl = static_cast<uint8_t>(strlen(name));
	const auto gil = static_cast<uint32_t>(FIELD_OFFSET(FILE_GET_EA_INFORMATION, EaName) + nl + 1);
	const auto pgi = create_fam_struct<FILE_GET_EA_INFORMATION>(gil);
	const auto il = static_cast<uint32_t>(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + sizeof(T) + nl + 1);
	const auto pi = create_fam_struct<FILE_FULL_EA_INFORMATION>(il);
	pgi->NextEntryOffset = 0;
	pgi->EaNameLength = nl;
	strcpy(pgi->EaName, name);
	const auto stat = NtQueryEaFile(
		hf, &iostat,
		pi.get(), static_cast<uint32_t>(il), true,
		pgi.get(), static_cast<uint32_t>(gil), nullptr, true
	);
	if (stat) throw lro_error::from_nt(err_msg::err_get_ea, {}, stat);
	if (pi->EaValueLength != sizeof(T)) {
		throw lro_error::from_other(err_msg::err_invalid_ea, { from_utf8(name) });
	}
	T t = {};
	memcpy(&t, pi->EaName + nl + 1, sizeof(T));
	return t;
}

template<typename T>
void set_ea(const HANDLE hf, const char *name, const T &data) {
	const auto nl = static_cast<uint8_t>(strlen(name));
	const auto il = static_cast<uint32_t>(FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + sizeof(T) + nl + 1);
	const auto pi = create_fam_struct<FILE_FULL_EA_INFORMATION>(il);
	pi->NextEntryOffset = 0;
	pi->Flags = 0;
	pi->EaNameLength = nl;
	pi->EaValueLength = sizeof(T);
	strcpy(pi->EaName, name);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
	memcpy(pi->EaName + nl + 1, &data, sizeof(T));
#pragma GCC diagnostic pop
	const auto stat = NtSetEaFile(hf, &iostat, pi.get(), il);
	if (stat) throw lro_error::from_nt(err_msg::err_set_ea, { from_utf8(name) }, stat);
}

void find_close_safe(const HANDLE hs) {
	if (hs != INVALID_HANDLE_VALUE) FindClose(hs);
}

void enum_directory(file_path &path, const bool rootfs_first, std::function<void(enum_dir_type)> action) {
	std::function<void(bool)> enum_rec;
	enum_rec = [&](const bool is_root) {
		try {
			const auto hf = open_file(path.data, true, false);
			if (get_win_build() <= 20206) {
				set_cs_info(hf.get());
			}
		} catch (lro_error &e) {
			if (e.msg_code == err_msg::err_set_cs) e.msg_args.push_back(path.data);
			throw;
		}
		action(enum_dir_type::enter);
		const auto os = path.data.size();
		if (is_root) {
			path.data += L"rootfs\\";
			enum_rec(false);
			path.data.resize(os);
		}
		WIN32_FIND_DATA data;
		path.data += L'*';
		const unique_ptr_del<HANDLE> hs(FindFirstFile(path.data.c_str(), &data), &find_close_safe);
		path.data.resize(os);
		if (hs.get() == INVALID_HANDLE_VALUE) {
			throw lro_error::from_win32_last(err_msg::err_enum_dir, { path.data });
		}
		while (true) {
			if (wcscmp(data.cFileName, L".") != 0 && wcscmp(data.cFileName, L"..") != 0
				&& (!is_root || wcscmp(data.cFileName, L"rootfs") != 0)) {

				path.data += data.cFileName;
				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					path.data += L'\\';
					enum_rec(false);
				} else {
					action(enum_dir_type::file);
				}
				path.data.resize(os);
			}
			if (!FindNextFile(hs.get(), &data)) {
				if (GetLastError() == ERROR_NO_MORE_FILES) {
					action(enum_dir_type::exit);
					return;
				}
				throw lro_error::from_win32_last(err_msg::err_enum_dir, { path.data });
			}
		}
	};
	enum_rec(rootfs_first);
}

void time_u2f(const unix_time &ut, LARGE_INTEGER &ft) {
	ft.QuadPart = ut.sec * 10000000 + ut.nsec / 100 + 116444736000000000;
}

void time_f2u(const LARGE_INTEGER &ft, unix_time &ut) {
	const auto t = ft.QuadPart - 116444736000000000;
	ut.sec = static_cast<uint64_t>(t / 10000000);
	ut.nsec = static_cast<uint32_t>(t % 10000000 * 100);
}

bool fs_writer::check_attr(const file_attr *attr, const bool allow_null, const bool allow_sock) {
	if (attr) {
		const auto type = attr->mode & AE_IFMT;
		if (type == AE_IFREG || type == AE_IFLNK || type == AE_IFCHR || type == AE_IFBLK || type == AE_IFDIR || type == AE_IFIFO) return true;
		if (type == AE_IFSOCK && allow_sock) return true;
		log_warning((boost::wformat(L"Ignoring an unsupported file \"%1%\" of type %2$07o.") % path->data % type).str());
	} else if (allow_null) {
		return true;
	} else {
		log_warning((boost::wformat(L"Ignoring the file \"%1%\" which doesn't have WSL attributes.") % path->data).str());
	}
	ignored_files.insert(path->data);
	return false;
}

bool fs_writer::check_target_ignored() {
	if (ignored_files.find(target_path->data) != ignored_files.end()) {
		log_warning((boost::wformat(L"Ignoring the hard link \"%1%\" whose target has been ignored.") % path->data).str());
		ignored_files.insert(path->data);
		return false;
	}
	return true;
}

archive_writer::archive_writer(crwstr archive_path)
	: pa(archive_write_new(), &archive_write_free), pe(archive_entry_new(), &archive_entry_free) {
	path = std::make_unique<linux_path>();
	target_path = std::make_unique<linux_path>();
	check_archive(pa.get(), archive_write_set_format_gnutar(pa.get()));
	check_archive(pa.get(), archive_write_add_filter_gzip(pa.get()));
	check_archive(pa.get(), archive_write_open_filename_w(pa.get(), archive_path.c_str()));
}

bool archive_writer::write_new_file(const file_attr *attr) {
	if (!check_attr(attr, false, false) || path->data.empty()) return false;
	const auto up = to_utf8(path->data);
	const auto type = attr->mode & AE_IFMT;
	archive_entry_set_pathname(pe.get(), up.get());
	archive_entry_set_uid(pe.get(), attr->uid);
	archive_entry_set_gid(pe.get(), attr->gid);
	archive_entry_set_mode(pe.get(), static_cast<unsigned short>(attr->mode));
	archive_entry_set_size(pe.get(), attr->size);
	archive_entry_set_atime(pe.get(), attr->at.sec, attr->at.nsec);
	archive_entry_set_mtime(pe.get(), attr->mt.sec, attr->mt.nsec);
	archive_entry_set_ctime(pe.get(), attr->ct.sec, attr->ct.nsec);
	if (type == AE_IFLNK) {
		archive_entry_set_symlink(pe.get(), attr->symlink);
	} else if (type == AE_IFCHR || type == AE_IFBLK) {
		archive_entry_set_rdevmajor(pe.get(), static_cast<dev_t>(attr->dev_major));
		archive_entry_set_rdevminor(pe.get(), static_cast<dev_t>(attr->dev_minor));
	}
	check_archive(pa.get(), archive_write_header(pa.get(), pe.get()));
	archive_entry_clear(pe.get());
	return true;
}

void archive_writer::write_file_data(const char *buf, const uint32_t size) {
	if (size) {
		if (archive_write_data(pa.get(), buf, size) < 0) {
			check_archive(pa.get(), ARCHIVE_FATAL);
		}
	}
}

void archive_writer::write_hard_link() {
	if (!check_target_ignored()) return;
	const auto up = to_utf8(path->data);
	archive_entry_set_pathname(pe.get(), up.get());
	const auto ut = to_utf8(target_path->data);
	archive_entry_set_hardlink(pe.get(), ut.get());
	check_archive(pa.get(), archive_write_header(pa.get(), pe.get()));
	archive_entry_clear(pe.get());
}

bool archive_writer::check_source_path(const file_path &) const {
	return true;
}

wsl_writer::wsl_writer() : hf_data(nullptr) {}

void wsl_writer::write_data(const HANDLE hf, const char *buf, const uint32_t size) const {
	DWORD wc;
	if (!WriteFile(hf, buf, size, &wc, nullptr)) {
		throw lro_error::from_win32_last(err_msg::err_write_file, { path->data });
	}
}

bool wsl_writer::write_new_file(const file_attr *attr) {
	if (!check_attr(attr, true, true)) return false;
	const auto type = attr ? attr->mode & AE_IFMT : AE_IFREG;
	const auto is_dir = type == AE_IFDIR;
	if (is_dir) {
		if (!CreateDirectory(path->data.c_str(), nullptr)) {
			const auto e = lro_error::from_win32_last(err_msg::err_create_dir, { path->data });
			if (GetLastError() != ERROR_ALREADY_EXISTS) throw lro_error(e);
			log_warning(e.format());
		}
	}
	auto hf = open_file(path->data, is_dir, !is_dir);
	write_attr(hf.get(), attr);
	if (type == AE_IFREG) {
		hf_data = std::move(hf);
	} else if (type == AE_IFDIR) {
		try {
			set_cs_info(hf.get());
		} catch (lro_error &e) {
			e.msg_args.push_back(path->data);
			throw;
		}
	}
	return true;
}

void wsl_writer::write_file_data(const char *buf, const uint32_t size) {
	if (size) write_data(hf_data.get(), buf, size);
	else hf_data.reset();
}

void wsl_writer::write_hard_link() {
	if (!check_target_ignored()) return;
	if (!CreateHardLink(path->data.c_str(), target_path->data.c_str(), nullptr)) {
		throw lro_error::from_win32_last(err_msg::err_hard_link, { path->data, target_path->data });
	}
}

bool wsl_writer::check_source_path(const file_path &sp) const {
	// base_len of a linux_path is always 0, so it will be safely ignored.
	return path->data.compare(0, std::min(path->base_len, sp.base_len), sp.data, 0, sp.base_len);
}

wsl_v1_writer::wsl_v1_writer(crwstr base_path) {
	path = std::make_unique<wsl_v1_path>(base_path);
	target_path = std::make_unique<wsl_v1_path>(base_path);
	create_recursive(path->data);
}

void wsl_v1_writer::write_attr(const HANDLE hf, const file_attr *attr) {
	if (!attr) return;
	try {
		set_ea(hf, "LXATTRB", lxattrb {
			0, 1,
			attr->mode, attr->uid, attr->gid,
			attr->dev_major << 20 | (attr->dev_minor & 0xfffff),
			attr->at.nsec, attr->mt.nsec, attr->ct.nsec,
			attr->at.sec, attr->mt.sec, attr->ct.sec
		});
	} catch (lro_error &e) {
		e.msg_args.push_back(path->data);
		throw;
	}
	if ((attr->mode & AE_IFMT) == AE_IFLNK) {
		write_data(hf, attr->symlink, static_cast<uint32_t>(strlen(attr->symlink)));
	}
}

wsl_v2_writer::wsl_v2_writer(crwstr base_path) {
	path = std::make_unique<wsl_v2_path>(base_path);
	target_path = std::make_unique<wsl_v2_path>(base_path);
	create_recursive(path->data);
}

void wsl_v2_writer::real_write_attr(const HANDLE hf, const file_attr &attr, crwstr path) {
	const auto type = attr.mode & AE_IFMT;

	try {
		set_ea(hf, "$LXUID", attr.uid);
		set_ea(hf, "$LXGID", attr.gid);
		set_ea(hf, "$LXMOD", attr.mode);
		if (type == AE_IFCHR || type == AE_IFBLK) {
			set_ea(hf, "$LXDEV", static_cast<uint64_t>(attr.dev_minor) << 32 | attr.dev_major);
		}
	} catch (lro_error &e) {
		e.msg_args.push_back(path);
		throw;
	}

	unique_ptr_del<REPARSE_DATA_BUFFER *> pb = nullptr;
	const auto hl = FIELD_OFFSET(REPARSE_DATA_BUFFER, DataBuffer);
	if (type == AE_IFLNK) {
		const uint32_t v = 2;
		const auto pl = strlen(attr.symlink);
		const auto dl = static_cast<uint16_t>(pl + sizeof(v));
		const auto bl = static_cast<uint32_t>(hl + dl);
		pb = create_fam_struct<REPARSE_DATA_BUFFER>(bl);
		pb->ReparseTag = IO_REPARSE_TAG_LX_SYMLINK;
		pb->ReparseDataLength = dl;
		memcpy(pb->DataBuffer, &v, sizeof(v));
		memcpy(pb->DataBuffer + sizeof(v), attr.symlink, pl);
	} else if (type == AE_IFSOCK) {
		pb = create_fam_struct<REPARSE_DATA_BUFFER>(hl);
		pb->ReparseTag = IO_REPARSE_TAG_AF_UNIX;
		pb->ReparseDataLength = 0;
	} else if (type == AE_IFCHR) {
		pb = create_fam_struct<REPARSE_DATA_BUFFER>(hl);
		pb->ReparseTag = IO_REPARSE_TAG_LX_CHR;
		pb->ReparseDataLength = 0;
	} else if (type == AE_IFBLK) {
		pb = create_fam_struct<REPARSE_DATA_BUFFER>(hl);
		pb->ReparseTag = IO_REPARSE_TAG_LX_BLK;
		pb->ReparseDataLength = 0;
	} else if (type == AE_IFIFO) {
		pb = create_fam_struct<REPARSE_DATA_BUFFER>(hl);
		pb->ReparseTag = IO_REPARSE_TAG_LX_FIFO;
		pb->ReparseDataLength = 0;
	}
	if (pb) {
		pb->Reserved = 0;
		const auto bl = hl + pb->ReparseDataLength;
		DWORD cnt;
		if (!DeviceIoControl(hf, FSCTL_SET_REPARSE_POINT,
			pb.get(), bl, nullptr, 0, &cnt, nullptr)) {

			throw lro_error::from_win32_last(err_msg::err_set_reparse, { path });
		}
	}

	FILE_BASIC_INFO info;
	time_u2f(attr.at, info.LastAccessTime);
	time_u2f(attr.mt, info.LastWriteTime);
	time_u2f(attr.ct, info.ChangeTime);
	info.CreationTime = info.ChangeTime;
	info.FileAttributes = 0;
	if (!SetFileInformationByHandle(hf, FileBasicInfo, &info, sizeof(FILE_BASIC_INFO))) {
		throw lro_error::from_win32_last(err_msg::err_set_ft, { path });
	}
}

void wsl_v2_writer::write_attr(const HANDLE hf, const file_attr *attr) {
	if (!attr) return;
	if ((attr->mode & AE_IFMT) == AE_IFDIR) {
		while (!dir_attr.empty()) {
			auto p = dir_attr.top();
			if (!path->data.compare(0, p.first.size(), p.first)) break;
			dir_attr.pop();
			real_write_attr(open_file(p.first, true, false).get(), p.second, p.first);
		}
		dir_attr.push(std::make_pair(path->data, *attr));
	} else real_write_attr(hf, *attr, path->data);
}

wsl_v2_writer::~wsl_v2_writer() {
	try {
		while (!dir_attr.empty()) {
			auto p = dir_attr.top();
			dir_attr.pop();
			real_write_attr(open_file(p.first, true, false).get(), p.second, p.first);
		}
	} catch (const lro_error &e) {
		log_error(e.format());
	} catch (const std::exception &e) {
		log_error(from_utf8(e.what()));
	}
}

wsl_legacy_writer::wsl_legacy_writer(crwstr base_path) {
	path = std::make_unique<wsl_legacy_path>(base_path);
	target_path = std::make_unique<wsl_legacy_path>(base_path);
	create_recursive(path->data);
}

archive_reader::archive_reader(wstr archive_path, wstr root_path)
	: archive_path(std::move(archive_path)), root_path(std::move(root_path)) {}

void archive_reader::run(fs_writer &writer) {
	auto as = get_file_size(open_file(archive_path, false, false).get());
	unique_ptr_del<archive *> pa(archive_read_new(), &archive_read_free);
	check_archive(pa.get(), archive_read_support_filter_all(pa.get()));
	check_archive(pa.get(), archive_read_support_format_all(pa.get()));
	check_archive(pa.get(), archive_read_open_filename_w(pa.get(), archive_path.c_str(), BUFSIZ));
	linux_path p;
	if (p.convert(*writer.path)) {
		file_attr attr { 0040755, 0, 0, 0, {}, {}, {}, 0, 0, nullptr };
		writer.write_new_file(&attr);
	}
	archive_entry *pe;
	while (check_archive(pa.get(), archive_read_next_header(pa.get(), &pe))) {
		print_progress(static_cast<double>(archive_filter_bytes(pa.get(), -1)) / as);
		auto up = archive_entry_pathname(pe);
		auto wp = archive_entry_pathname_w(pe);
		if (up) p = linux_path(from_utf8(up), root_path);
		else if (wp) p = linux_path(wp, root_path);
		else throw lro_error::from_other(err_msg::err_convert_encoding, {});
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
		auto pst = archive_entry_stat(pe);
		unix_time mt {
			static_cast<uint64_t>(pst->st_mtime),
			static_cast<uint32_t>(archive_entry_mtime_nsec(pe))
		};
		file_attr attr {
			static_cast<uint32_t>(pst->st_mode),
			static_cast<uint32_t>(pst->st_uid),
			static_cast<uint32_t>(pst->st_gid),
			static_cast<uint64_t>(pst->st_size),
			archive_entry_atime_is_set(pe) ? unix_time {
				static_cast<uint64_t>(pst->st_atime),
				static_cast<uint32_t>(archive_entry_atime_nsec(pe))
			} : mt,
			mt,
			archive_entry_ctime_is_set(pe) ? unix_time {
				static_cast<uint64_t>(pst->st_ctime),
				static_cast<uint32_t>(archive_entry_ctime_nsec(pe))
			} : mt,
			static_cast<uint32_t>(archive_entry_rdevmajor(pe)),
			static_cast<uint32_t>(archive_entry_rdevminor(pe)),
			nullptr
		};
		std::unique_ptr<char[]> ptp;
		if (type == AE_IFLNK) {
			attr.symlink = archive_entry_symlink(pe);
			if (!attr.symlink) {
				ptp = to_utf8(archive_entry_symlink_w(pe));
				attr.symlink = ptp.get();
			}
		}
		if (!writer.write_new_file(&attr)) continue;
		if (type == AE_IFREG) {
			const void *buf;
			size_t cnt;
			int64_t off;
			while (check_archive(pa.get(), archive_read_data_block(pa.get(), &buf, &cnt, &off))) {
				writer.write_file_data(static_cast<const char *>(buf), static_cast<uint32_t>(cnt));
			}
			writer.write_file_data(nullptr, 0);
		}
	}
}

bool wsl_reader::is_legacy() const {
	return false;
}

void wsl_reader::run(fs_writer &writer) {
	std::map<uint64_t, std::unique_ptr<file_path>> id_map;
	char buf[BUFSIZ];
	auto is_root = true;
	enum_directory(*path, is_legacy(), [&](const enum_dir_type t) {
		if (t == enum_dir_type::exit) return;
		if (t == enum_dir_type::enter && is_root) {
			is_root = false;
			return;
		}
		if (!path->convert(*writer.path)) return;
		const auto dir = t == enum_dir_type::enter;
		const auto hf = open_file(path->data, dir, false);
		if (!dir) {
			BY_HANDLE_FILE_INFORMATION info;
			if (!GetFileInformationByHandle(hf.get(), &info)) {
				throw lro_error::from_win32_last(err_msg::err_file_info, { path->data });
			}
			if (info.nNumberOfLinks > 1) {
				const auto id = info.nFileIndexLow + (static_cast<uint64_t>(info.nFileIndexHigh) << 32);
				if (id_map.count(id)) {
					if (id_map[id]->convert(*writer.target_path)) writer.write_hard_link();
					return;
				} else id_map[id] = path->clone();
			}
		}
		const auto attr = read_attr(hf.get());
		if (dir) writer.write_new_file(attr.get());
		else {
			const auto type = attr ? attr->mode & AE_IFMT : AE_IFREG;
			std::unique_ptr<char[]> tb;
			if (type == AE_IFLNK) {
				tb = read_symlink_data(hf.get());
				if (attr && tb) attr->symlink = tb.get();
				else {
					log_warning((boost::wformat(L"Ignoring an invalid symlink \"%1%\".") % path->data).str());
					return;
				}
			}
			if (!writer.write_new_file(attr.get())) return;
			if (type == AE_IFREG) {
				DWORD rc;
				do {
					if (!ReadFile(hf.get(), buf, BUFSIZ, &rc, nullptr)) {
						throw lro_error::from_win32_last(err_msg::err_read_file, { path->data });
					}
					writer.write_file_data(buf, rc);
				} while (rc);
			}
		}
	});
}

void wsl_reader::run_checked(fs_writer &writer) {
	if (!writer.check_source_path(*path)) {
		throw lro_error::from_other(err_msg::err_copy_subdir, {});
	}
	run(writer);
}

wsl_v1_reader::wsl_v1_reader(crwstr base) {
	path = std::make_unique<wsl_v1_path>(base);
}

std::unique_ptr<file_attr> wsl_v1_reader::read_attr(const HANDLE hf) const {
	try {
		const auto ea = get_ea<lxattrb>(hf, "LXATTRB");
		return std::make_unique<file_attr>(file_attr {
			ea.mode, ea.uid, ea.gid, get_file_size(hf),
			{ ea.atime, ea.atime_nsec },
			{ ea.mtime, ea.mtime_nsec },
			{ ea.ctime, ea.ctime_nsec },
			ea.rdev >> 20, ea.rdev & 0xfffff,
			nullptr
		});
	} catch (lro_error &e) {
		if (e.msg_code == err_msg::err_invalid_ea) return nullptr;
		e.msg_args.push_back(path->data);
		throw;
	}
}

std::unique_ptr<char[]> wsl_v1_reader::read_symlink_data(const HANDLE hf) const {
	uint64_t sz;
	try {
		sz = get_file_size(hf);
	} catch (lro_error &e) {
		e.msg_args.push_back(path->data);
		throw;
	}
	if (sz > 65536) throw lro_error::from_other(err_msg::err_symlink_length, { path->data, std::to_wstring(sz) });
	auto buf = std::make_unique<char[]>(sz + 1);
	DWORD rc;
	for (uint32_t off = 0; off < sz; off += rc) {
		if (!ReadFile(hf, buf.get() + off, static_cast<uint32_t>(sz - off), &rc, nullptr)) {
			throw lro_error::from_win32_last(err_msg::err_read_file, { path->data });
		}
	}
	buf[sz] = 0;
	return buf;
}

wsl_v2_reader::wsl_v2_reader(crwstr base) {
	path = std::make_unique<wsl_v2_path>(base);
}

std::unique_ptr<file_attr> wsl_v2_reader::read_attr(const HANDLE hf) const {
	std::unique_ptr<file_attr> attr(new file_attr);
	try {
		attr->uid = get_ea<uint32_t>(hf, "$LXUID");
		attr->gid = get_ea<uint32_t>(hf, "$LXGID");
		attr->mode = get_ea<uint32_t>(hf, "$LXMOD");
		attr->size = get_file_size(hf);
		const auto type = attr->mode & AE_IFMT;
		if (type == AE_IFCHR || type == AE_IFBLK) {
			const auto dev = get_ea<uint64_t>(hf, "$LXDEV");
			attr->dev_major = static_cast<uint32_t>(dev);
			attr->dev_minor = static_cast<uint32_t>(dev >> 32);
		}
	} catch (lro_error &e) {
		if (e.msg_code == err_msg::err_invalid_ea) return nullptr;
		e.msg_args.push_back(path->data);
		throw;
	}
	FILE_BASIC_INFO info;
	if (!GetFileInformationByHandleEx(hf, FileBasicInfo, &info, sizeof info)) {
		throw lro_error::from_win32_last(err_msg::err_get_ft, { path->data });
	}
	time_f2u(info.LastAccessTime, attr->at);
	time_f2u(info.LastWriteTime, attr->mt);
	time_f2u(info.ChangeTime, attr->ct);
	return attr;
}

std::unique_ptr<char[]> wsl_v2_reader::read_symlink_data(const HANDLE hf) const {
	const auto pb = create_fam_struct<REPARSE_DATA_BUFFER>(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
	DWORD cnt;
	if (!DeviceIoControl(hf, FSCTL_GET_REPARSE_POINT,
		nullptr, 0, pb.get(), MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &cnt, nullptr)) {

		if (GetLastError() == ERROR_NOT_A_REPARSE_POINT) return nullptr;
		throw lro_error::from_win32_last(err_msg::err_get_reparse, { path->data });
	}
	if (pb->ReparseTag != IO_REPARSE_TAG_LX_SYMLINK) return nullptr;
	const auto pl = pb->ReparseDataLength - 4;
	auto s = std::make_unique<char[]>(static_cast<size_t>(pl) + 1);
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
bool has_ea(crwstr path, const char *name, const bool ignore_error) {
	try {
		get_ea<T>(open_file(path, true, false).get(), name);
		return true;
	} catch (const lro_error &e) {
		if (e.msg_code == err_msg::err_invalid_ea || ignore_error) return false;
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
	throw lro_error::from_other(err_msg::err_fs_detect, { path });
}

bool detect_wsl2(crwstr path) {
	const wsl_v2_path p(path);
	try {
		open_file(p.data + L"ext4.vhdx", false, false);
		return true;
	} catch (const lro_error &e) {
		if (e.msg_code == err_msg::err_open_file && e.err_code == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			return false;
		}
		throw;
	}
}

std::unique_ptr<wsl_writer> select_wsl_writer(const uint32_t version, crwstr path) {
	if (version == 0) return std::make_unique<wsl_legacy_writer>(path);
	if (version == 1) return std::make_unique<wsl_v1_writer>(path);
	if (version == 2) return std::make_unique<wsl_v2_writer>(path);
	throw lro_error::from_other(err_msg::err_fs_version, { std::to_wstring(version) });
}

std::unique_ptr<wsl_reader> select_wsl_reader(const uint32_t version, crwstr path) {
	if (version == 0) return std::make_unique<wsl_legacy_reader>(path);
	if (version == 1) return std::make_unique<wsl_v1_reader>(path);
	if (version == 2) return std::make_unique<wsl_v2_reader>(path);
	throw lro_error::from_other(err_msg::err_fs_version, { std::to_wstring(version) });
}

bool move_directory(crwstr source_path, crwstr target_path) {
	return MoveFile(source_path.c_str(), target_path.c_str());
}

void delete_directory(crwstr path) {
	wsl_v2_path p(path);
	enum_directory(p, false, [&](const enum_dir_type t) {
		if (t == enum_dir_type::enter) return;
		const auto dir = t == enum_dir_type::exit;
		if (!(dir ? RemoveDirectory : DeleteFile)(p.data.c_str())) {
			throw lro_error::from_win32_last(dir ? err_msg::err_delete_dir : err_msg::err_delete_file, { p.data });
		}
	});
}

bool check_in_use(crwstr path) {
	try {
		open_file(path, false, false, true);
	} catch (const lro_error &e) {
		if (e.msg_code == err_msg::err_open_file && e.err_code == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)) {
			return true;
		}
	}
	return false;
}
