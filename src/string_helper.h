/*
Copyright (c) 2020 Maarten Baert <info@maartenbaert.be>

This file is part of lowrider.

lowrider is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

lowrider is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with lowrider.  If not, see <http://www.gnu.org/licenses/>.
*/

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
