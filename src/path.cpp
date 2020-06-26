#include "stdafx.h"
#include "path.h"
#include "utils.h"

prefix_matcher::prefix_matcher(std::initializer_list<wstr> patterns)
	: done(false), pos(0) {
	trie.resize(1);
	for (crwstr s : patterns) {
		size_t p = 0;
		for (size_t i = 0; i < s.size() - 1; i++) {
			auto &m = trie[p];
			auto it = m.find(s[i]);
			if (it == m.end()) {
				p = m[s[i]] = trie.size();
				trie.resize(trie.size() + 1);
			} else p = it->second;
		}
		trie[p][s.back()] = 0;
	}
}

match_result prefix_matcher::move(const wchar_t c) {
	if (done) return match_result::unknown;
	auto &m = trie[pos];
	const auto it = m.find(c);
	if (it == m.end()) {
		done = true;
		return match_result::failed;
	}
	if (it->second == 0) {
		done = true;
		return match_result::succeeded;
	}
	pos = it->second;
	return match_result::unknown;
}

void prefix_matcher::reset() {
	done = false;
	pos = 0;
}

file_path::file_path(crwstr path)
	: base_len(path.size()), data(path) {}

bool file_path::append(crwstr s) {
	for (auto c : s) {
		if (!append(c)) return false;
	}
	return true;
}

void file_path::reset() {
	data.resize(base_len);
}

linux_path::linux_path()
	: file_path(L""), skip(false), matcher({ L"rootfs/" }) {}

linux_path::linux_path(crwstr path, crwstr root_path) : linux_path() {
	size_t pos = 0;
	if (!root_path.empty()) {
		if (path.compare(0, root_path.size(), root_path)) skip = true;
		else {
			if (root_path.back() != '/') {
				if (pos < path.size() && path[pos + root_path.size()] == '/') pos += root_path.size() + 1;
			} else pos += root_path.size();
			if (pos == path.size()) skip = true;
		}
		if (skip) return;
	}
	auto cb = true;
	while (pos < path.size()) {
		if (cb) {
			if (path[pos] == L'/') {
				pos++;
				continue;
			}
			if (!path.compare(pos, 2, L"./")) {
				pos += 2;
				continue;
			}
			if (!path.compare(pos, 3, L"../")) {
				pos += 3;
				if (!data.empty()) {
					const auto sp = data.rfind(L'/', data.size() - 2);
					if (sp == wstr::npos) data.clear();
					else data.resize(sp + 1);
				}
				continue;
			}
		}
		cb = path[pos] == L'/';
		data += path[pos++];
	}
	skip = data.empty();
}

bool linux_path::append(const wchar_t c) {
	switch (matcher.move(c)) {
	case match_result::failed:
		return false;
	case match_result::succeeded:
		data.clear();
		break;
	case match_result::unknown:
		data += c;
		break;
	}
	return true;
}

bool linux_path::convert(file_path &output) const {
	if (skip) return false;
	output.reset();
	return output.append(L"rootfs/") && output.append(data);
}

void linux_path::reset() {
	file_path::reset();
	matcher.reset();
}

std::unique_ptr<file_path> linux_path::clone() const {
	return std::make_unique<linux_path>(*this);
}

wsl_path::wsl_path(crwstr base) : file_path(normalize_path(base)) {}

wstr wsl_path::normalize_path(crwstr path) {
	const auto prefix = L"\\\\?\\";
	const auto prefix_len = wcslen(prefix);
	auto o = get_full_path(path);
	if (o.compare(0, prefix_len, prefix) != 0) {
		o = prefix + get_full_path(path);
	}
	if (o.back() != L'\\') o += L'\\';
	return o;
}

bool wsl_path::is_special_input(const wchar_t c) const {
	return c >= 1 && c <= 31 || c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'\\' || c == L'|' || c == L'*' || c == L'?';
}

bool wsl_path::real_convert(file_path &output) const {
	for (auto i = base_len; i < data.size(); i++) {
		wchar_t c;
		if (is_special_output(data[i])) {
			if (!convert_special(output, i)) return false;
		} else {
			if (data[i] == L'\\') c = L'/';
			else c = data[i];
			if (!output.append(c)) return false;
		}
	}
	return true;
}

bool wsl_path::append(const wchar_t c) {
	if (is_special_input(c)) append_special(c);
	else if (c == L'/') data += L'\\';
	else data += c;
	return true;
}

bool wsl_path::convert(file_path &output) const {
	output.reset();
	return real_convert(output);
}

wsl_v1_path::wsl_v1_path(crwstr base) : wsl_path(base) {}

void wsl_v1_path::append_special(const wchar_t c) {
	data += (boost::wformat(L"#%04X") % static_cast<uint16_t>(c)).str();
}

bool wsl_v1_path::convert_special(file_path &output, size_t &i) const {
	const auto res = output.append(static_cast<wchar_t>(stoi(data.substr(i + 1, 4), nullptr, 16)));
	i += 4;
	return res;
}

bool wsl_v1_path::is_special_input(const wchar_t c) const {
	return wsl_path::is_special_input(c) || c == L'#';
}

bool wsl_v1_path::is_special_output(const wchar_t c) const {
	return c == L'#';
}

std::unique_ptr<file_path> wsl_v1_path::clone() const {
	return std::make_unique<wsl_v1_path>(*this);
}

wsl_v2_path::wsl_v2_path(crwstr base) : wsl_path(base) {}

void wsl_v2_path::append_special(const wchar_t c) {
	data += c | 0xf000;
}

bool wsl_v2_path::convert_special(file_path &output, size_t &i) const {
	return output.append(data[i] ^ 0xf000);
}

bool wsl_v2_path::is_special_output(const wchar_t c) const {
	return is_special_input(c ^ 0xf000);
}

std::unique_ptr<file_path> wsl_v2_path::clone() const {
	return std::make_unique<wsl_v2_path>(*this);
}

wsl_legacy_path::wsl_legacy_path(crwstr base)
	: wsl_v1_path(base), matcher1({ L"home/", L"root/", L"mnt/" }), matcher2({ L"rootfs/home/", L"rootfs/root/", L"rootfs/mnt/" }) {}

bool wsl_legacy_path::append(const wchar_t c) {
	if (!wsl_v1_path::append(c)) return false;
	if (matcher1.move(c) == match_result::succeeded) {
		// Maybe add warning
		return false;
	} else if (matcher2.move(c) == match_result::succeeded) {
		data.erase(base_len, 7);
	}
	return true;
}

bool wsl_legacy_path::convert(file_path &output) const {
	if (!data.compare(base_len, 12, L"rootfs\\root\\") || !data.compare(base_len, 12, L"rootfs\\home\\") || !data.compare(base_len, 11, L"rootfs\\mnt\\")) {
		// Maybe add warning
		return false;
	}
	output.reset();
	if (!data.compare(base_len, 5, L"root\\") || !data.compare(base_len, 5, L"home\\") || !data.compare(base_len, 4, L"mnt\\")) {
		if (!output.append(L"rootfs/")) return false;
	}
	return real_convert(output);
}

void wsl_legacy_path::reset() {
	wsl_v1_path::reset();
	matcher1.reset();
	matcher2.reset();
}

std::unique_ptr<file_path> wsl_legacy_path::clone() const {
	return std::make_unique<wsl_legacy_path>(*this);
}
