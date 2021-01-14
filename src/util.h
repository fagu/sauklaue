#ifndef UTIL_H
#define UTIL_H

// Helpers for std::variant copied from https://en.cppreference.com/w/cpp/utility/variant/visit

#include <cassert>
#include <list>
#include <memory>
#include <variant>
#include <vector>

// helper type for the visitor #4
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Moves the given object into a one-element vector.
template <class T>
std::vector<T> move_into_vector(T&& x) {
	std::vector<T> v;
	v.push_back(std::move(x));
	return v;
}

// Filters the elements of a vector<F::in_type> on-demand through a function object F() to obtain something a little like vector<F::out_type>.
template <class F, class Container>
class AbstractView {
	typedef typename F::in_type T;

public:
	class iterator {
	public:
		explicit iterator(typename Container::const_iterator _it) :
		    it(_it) {
		}
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
		typename Container::const_iterator it;
	};

	explicit AbstractView(const Container& _v) :
	    v(_v) {
	}
	size_t size() const {
		return v.size();
	}
	bool empty() const {
		return v.empty();
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
	const Container& v;
};

// Filters the elements of a vector<F::in_type> on-demand through a function object F() to obtain something a little like vector<F::out_type>.
template <class F>
using VectorView = AbstractView<F, std::vector<typename F::in_type> >;
template <class F>
using ListView = AbstractView<F, std::list<typename F::in_type> >;

template <class... T>
using variant_unique = std::variant<std::unique_ptr<T>...>;

// Converts variant<U1,U2,...> to T by using the default conversion Ui -> T for each alternative Ui.
template <class T, class... U>
T convert_variant(const std::variant<U...>& s) {
	return std::visit([](const auto& s) -> T { return s; }, s);
}
// Converts variant<unique_ptr<T1>,...,unique_ptr<Tn> > into variant<T1*,...,Tn*>.
template <class... T>
inline std::variant<T*...> get(const variant_unique<T...>& s) {
	return std::visit(
	        [](const auto& p) -> std::variant<T*...> { return p.get(); }, s);
}

template <class T>
struct unique_to_ptr_helper {
	typedef std::unique_ptr<T> in_type;
	typedef T* out_type;
	out_type operator()(const in_type& p) {
		return p.get();
	}
};

// Turns a vector<unique_ptr<T> > into something a little like vector<T*>.
template <class T>
VectorView<unique_to_ptr_helper<T> > vector_unique_to_pointer(const std::vector<std::unique_ptr<T> >& v) {
	return VectorView<unique_to_ptr_helper<T> >(v);
}
// Turns a list<unique_ptr<T> > into something a little like list<T*>.
template <class T>
ListView<unique_to_ptr_helper<T> > list_unique_to_pointer(const std::list<std::unique_ptr<T> >& v) {
	return ListView<unique_to_ptr_helper<T> >(v);
}

#endif
