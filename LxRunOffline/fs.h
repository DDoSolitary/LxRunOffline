#pragma once
#include "stdafx.h"
#include "utils.h"

struct unix_time {
	uint64_t sec;
	uint32_t nsec;
};

struct file_attr {
	uint32_t mode, uid, gid;
	uint64_t size;
	unix_time at, mt, ct;
};

class fs_writer {
public:
	virtual void write_new_file(crwstr, const file_attr &) = 0;
	virtual void write_file_data(const char *, uint32_t) = 0;
	virtual void write_directory(crwstr, const file_attr &) = 0;
	virtual void write_symlink(crwstr, const file_attr &, const char *) = 0;
	virtual void write_hard_link(crwstr, crwstr) = 0;
};

class archive_writer : public fs_writer {
	unique_ptr_del<archive *> pa;
	unique_ptr_del<archive_entry *> pe;
	void write_entry(crwstr, const file_attr &);
public:
	archive_writer(crwstr);
	void write_new_file(crwstr, const file_attr &);
	void write_file_data(const char *, uint32_t);
	void write_directory(crwstr, const file_attr &);
	void write_symlink(crwstr, const file_attr &, const char *);
	void write_hard_link(crwstr, crwstr);
};

class wsl_writer : public fs_writer {
protected:
	wstr path;
	const size_t blen;
	unique_ptr_del<HANDLE> hf_data;
	void write_data(HANDLE, const char *, uint32_t) const;
	virtual void set_path(crwstr) = 0;
	virtual void write_attr(HANDLE, const file_attr &) = 0;
	virtual void write_symlink_data(HANDLE, const char *) const = 0;
public:
	wsl_writer(crwstr);
	virtual ~wsl_writer() = default;
	void write_new_file(crwstr, const file_attr &);
	void write_file_data(const char *, uint32_t);
	void write_directory(crwstr, const file_attr &);
	void write_symlink(crwstr, const file_attr &, const char *);
	void write_hard_link(crwstr, crwstr);
};

class wsl_v1_writer : public wsl_writer {
protected:
	void set_path(crwstr);
	void write_attr(HANDLE, const file_attr &);
	void write_symlink_data(HANDLE, const char *) const;
public:
	using wsl_writer::wsl_writer;
};

class wsl_v2_writer : public wsl_writer {
	std::stack<std::pair<wstr, file_attr>> dir_attr;
	void real_write_attr(HANDLE, const file_attr &, crwstr) const;
protected:
	void set_path(crwstr);
	void write_attr(HANDLE, const file_attr &);
	void write_symlink_data(HANDLE, const char *) const;
public:
	using wsl_writer::wsl_writer;
	~wsl_v2_writer();
};

class fs_reader {
public:
	virtual void run(fs_writer &writer) = 0;
};

class archive_reader : public fs_reader {
	const wstr archive_path, root_path;
public:
	archive_reader(crwstr, crwstr);
	void run(fs_writer &);
};

class wsl_reader : public fs_reader {
protected:
	wstr path;
	const size_t blen;
	virtual wstr convert_path(crwstr) const = 0;
	virtual file_attr read_attr(HANDLE) const = 0;
	virtual std::unique_ptr<char[]> read_symlink_data(HANDLE) const = 0;
public:
	wsl_reader(crwstr);
	void run(fs_writer &);
};

class wsl_v1_reader : public wsl_reader {
protected:
	wstr convert_path(crwstr) const;
	file_attr read_attr(HANDLE) const;
	std::unique_ptr<char[]> read_symlink_data(HANDLE) const;
public:
	using wsl_reader::wsl_reader;
};

class wsl_v2_reader : public wsl_reader {
protected:
	wstr convert_path(crwstr) const;
	file_attr read_attr(HANDLE) const;
	std::unique_ptr<char[]> read_symlink_data(HANDLE) const;
public:
	using wsl_reader::wsl_reader;
};

uint32_t detect_version(crwstr path);
std::unique_ptr<wsl_writer> select_wsl_writer(uint32_t version, crwstr path);
std::unique_ptr<wsl_reader> select_wsl_reader(uint32_t version, crwstr path);
bool move_directory(crwstr source_path, crwstr target_path);
void delete_directory(crwstr path);
