#ifndef UTIL_H
#define UTIL_H

// Helpers for std::variant copied from https://en.cppreference.com/w/cpp/utility/variant/visit

#include <cairomm/context.h>

// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// RAII for the cairo context: Saves the current state on construction and restores it on deletion.
class CairoGroup {
public:
	CairoGroup(Cairo::RefPtr<Cairo::Context> cr) : _cr(cr) {
		_cr->save();
	}
	CairoGroup(const CairoGroup&) = delete;
	CairoGroup& operator=(const CairoGroup&) = delete;
	~CairoGroup() {
		_cr->restore();
	}
private:
	Cairo::RefPtr<Cairo::Context> _cr;
};

#endif
