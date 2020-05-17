#pragma once
#include "stdafx.h"

enum class match_result {
	failed,
	succeeded,
	unknown
};

// A pattern can't be a substring of another one
class prefix_matcher {
	std::vector<std::map<wchar_t, size_t>> trie;
	bool done;
	size_t pos;
public:
	prefix_matcher(std::initializer_list<wstr>);
	match_result move(wchar_t);
	void reset();
};

class file_path {
protected:
	explicit file_path(crwstr);
public:
	size_t base_len;
	wstr data;
	virtual ~file_path() = default;
	virtual bool append(wchar_t) = 0;
	bool append(crwstr);
	virtual bool convert(file_path &) const = 0;
	virtual void reset();
	virtual std::unique_ptr<file_path> clone() const = 0;
};

class linux_path : public file_path {
	bool skip;
	prefix_matcher matcher;
public:
	linux_path();
	linux_path(crwstr, crwstr);
	bool append(wchar_t) override;
	bool convert(file_path &) const override;
	void reset() override;
	std::unique_ptr<file_path> clone() const override;
};

class wsl_path : public file_path {
	static wstr normalize_path(crwstr);
protected:
	explicit wsl_path(crwstr);
	virtual void append_special(wchar_t) = 0;
	virtual bool convert_special(file_path &, size_t &) const = 0;
	virtual bool is_special_input(wchar_t) const;
	virtual bool is_special_output(wchar_t c) const = 0;
	bool real_convert(file_path &) const;
public:
	bool append(wchar_t) override;
	bool convert(file_path &) const override;
};

class wsl_v1_path : public wsl_path {
protected:
	void append_special(wchar_t) override;
	bool convert_special(file_path &, size_t &) const override;
	bool is_special_input(wchar_t) const override;
	bool is_special_output(wchar_t c) const override;
public:
	explicit wsl_v1_path(crwstr);
	std::unique_ptr<file_path> clone() const override;
};

class wsl_v2_path : public wsl_path {
protected:
	void append_special(wchar_t) override;
	bool convert_special(file_path &, size_t &) const override;
	bool is_special_output(wchar_t c) const override;
public:
	wsl_v2_path(crwstr);
	std::unique_ptr<file_path> clone() const override;
};

class wsl_legacy_path : public wsl_v1_path {
	prefix_matcher matcher1, matcher2;
public:
	explicit wsl_legacy_path(crwstr);
	bool append(wchar_t) override;
	bool convert(file_path &) const override;
	void reset() override;
	std::unique_ptr<file_path> clone() const override;
};
