#ifndef UTIL_H
#define UTIL_H

// Helpers for std::variant copied from https://en.cppreference.com/w/cpp/utility/variant/visit

#include <memory>
#include <vector>

#include <cairomm/context.h>

// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// Filters the elements of a vector<F::in_type> on-demand through a function object F() to obtain something a little like vector<F::out_type>.
template<class F>
class VectorView {
	typedef typename F::in_type T;
public:
	class iterator {
	public:
		explicit iterator(typename std::vector<T>::const_iterator _it) : it(_it) {}
		typename F::out_type operator*() const {
			return F()(*it);
		}
		iterator& operator++() {
			++it;
			return *this;
		}
		friend bool operator!=(const iterator& a, const iterator& b) {
			return a.it != b.it;
		}
	private:
		typename std::vector<T>::const_iterator it;
	};
	
	explicit VectorView(const std::vector<T> &_v) : v(_v) {}
	size_t size() const {
		return v.size();
	}
	auto operator[](size_t i) const {
		return F()(v[i]);
	}
	iterator begin() const {
		return iterator(v.cbegin());
	}
	iterator end() const {
		return iterator(v.cend());
	}
	
private:
	const std::vector<T> &v;
};

template<class T>
struct unique_to_ptr_helper {
	typedef std::unique_ptr<T> in_type;
	typedef T* out_type;
	out_type operator()(const in_type &p) {
		return p.get();
	}
};

// Turns a vector<unique_ptr<T> > into something a little like vector<T*>.
template<class T>
VectorView<unique_to_ptr_helper<T> > vector_unique_to_pointer(const std::vector<std::unique_ptr<T> > &v) {
	return VectorView<unique_to_ptr_helper<T> >(v);
}

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
