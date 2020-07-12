#pragma once

#include <sstream>
#include <string>

inline void extend_string(std::ostringstream&) {}

template<typename V, typename... Args>
inline void extend_string(std::ostringstream &ss, V &&value, Args&&... args) {
	ss << std::forward<V>(value);
	extend_string(ss, std::forward<Args>(args)...);
}

template<typename... Args>
inline std::string make_string(Args&&... args) {
	std::ostringstream ss;
	extend_string(ss, std::forward<Args>(args)...);
	return ss.str();
}

inline std::string to_lower(const std::string &str) {
	std::string res = str;
	for(char &c : res) {
		if(c >= 'A' && c <= 'Z') {
			c = c - 'A' + 'a';
		}
	}
	return res;
}

inline std::string to_upper(const std::string &str) {
	std::string res = str;
	for(char &c : res) {
		if(c >= 'a' && c <= 'z') {
			c = c - 'a' + 'A';
		}
	}
	return res;
}
