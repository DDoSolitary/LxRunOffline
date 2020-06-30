#pragma once
#include "stdafx.h"
#include "path.h"

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
	std::unique_ptr<file_path> path, target_path;
	virtual ~fs_writer() = default;
	virtual bool write_new_file(const file_attr *) = 0;
	virtual void write_file_data(const char *, uint32_t) = 0;
	virtual void write_directory(const file_attr *) = 0;
	virtual void write_symlink(const file_attr *, const char *) = 0;
	virtual void write_hard_link() = 0;
	[[nodiscard]] virtual bool check_source_path(const file_path &) const = 0;
};

class archive_writer : public fs_writer {
	unique_ptr_del<archive *> pa;
	unique_ptr_del<archive_entry *> pe;
	std::set<wstr> ignored_files;
	void write_entry(const file_attr &) const;
	bool check_attr(const file_attr *);
	static void warn_ignored(crwstr);
public:
	explicit archive_writer(crwstr);
	bool write_new_file(const file_attr *) override;
	void write_file_data(const char *, uint32_t) override;
	void write_directory(const file_attr *) override;
	void write_symlink(const file_attr *, const char *) override;
	void write_hard_link() override;
	[[nodiscard]] bool check_source_path(const file_path &) const override;
};

class wsl_writer : public fs_writer {
protected:
	unique_ptr_del<HANDLE> hf_data;
	void write_data(HANDLE, const char *, uint32_t) const;
	virtual void write_attr(HANDLE, const file_attr *) = 0;
	virtual void write_symlink_data(HANDLE, const char *) const = 0;
	wsl_writer();
public:
	bool write_new_file(const file_attr *) override;
	void write_file_data(const char *, uint32_t) override;
	void write_directory(const file_attr *) override;
	void write_symlink(const file_attr *, const char *) override;
	void write_hard_link() override;
	[[nodiscard]] bool check_source_path(const file_path &) const override;
};

class wsl_v1_writer : public wsl_writer {
protected:
	wsl_v1_writer() = default;
	void write_attr(HANDLE, const file_attr *) override;
	void write_symlink_data(HANDLE, const char *) const override;
public:
	explicit wsl_v1_writer(crwstr);
};

class wsl_v2_writer : public wsl_writer {
	std::stack<std::pair<wstr, file_attr>> dir_attr;
	static void real_write_attr(HANDLE, const file_attr &, crwstr);
protected:
	void write_attr(HANDLE, const file_attr *) override;
	void write_symlink_data(HANDLE, const char *) const override;
public:
	explicit wsl_v2_writer(crwstr);
	~wsl_v2_writer() override;
};

class wsl_legacy_writer : public wsl_v1_writer {
public:
	explicit wsl_legacy_writer(crwstr);
};

class fs_reader {
public:
	virtual ~fs_reader() = default;
	virtual void run(fs_writer &writer) = 0;
};

class archive_reader : public fs_reader {
	const wstr archive_path, root_path;
public:
	archive_reader(wstr, wstr);
	void run(fs_writer &) override;
};

class wsl_reader : public fs_reader {
protected:
	std::unique_ptr<file_path> path;
	virtual std::unique_ptr<file_attr> read_attr(HANDLE) const = 0;
	virtual std::unique_ptr<char[]> read_symlink_data(HANDLE) const = 0;
	[[nodiscard]] virtual bool is_legacy() const;
public:
	void run(fs_writer &) override;
	void run_checked(fs_writer &);
};

class wsl_v1_reader : public wsl_reader {
protected:
	wsl_v1_reader() = default;
	std::unique_ptr<file_attr> read_attr(HANDLE) const override;
	std::unique_ptr<char[]> read_symlink_data(HANDLE) const override;
public:
	explicit wsl_v1_reader(crwstr);
};

class wsl_v2_reader : public wsl_reader {
protected:
	std::unique_ptr<file_attr> read_attr(HANDLE) const override;
	std::unique_ptr<char[]> read_symlink_data(HANDLE) const override;
public:
	explicit wsl_v2_reader(crwstr);
};

class wsl_legacy_reader : public wsl_v1_reader {
protected:
	[[nodiscard]] bool is_legacy() const override;
public:
	explicit wsl_legacy_reader(crwstr);
};

uint32_t detect_version(crwstr path);
bool detect_wsl2(crwstr path);
std::unique_ptr<wsl_writer> select_wsl_writer(uint32_t version, crwstr path);
std::unique_ptr<wsl_reader> select_wsl_reader(uint32_t version, crwstr path);
bool move_directory(crwstr source_path, crwstr target_path);
void delete_directory(crwstr path);
bool check_in_use(crwstr path);
