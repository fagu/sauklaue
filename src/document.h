#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "util.h"

#include <memory>
#include <variant>
#include <vector>

#include <QColor>
#include <QString>
#include <QDataStream>

class Color {
public:
	constexpr Color(uint32_t _x) : x(_x) {}
	Color(QColor c) : Color(color(c.red(),c.green(),c.blue(),255)) {}
	
	uint32_t x;
	
	static constexpr uint32_t MASK = 255;
	
	double r() const {
		return (double)((x>>24)&MASK)/MASK;
	}
	double g() const {
		return (double)((x>>16)&MASK)/MASK;
	}
	double b() const {
		return (double)((x>>8)&MASK)/MASK;
	}
	double a() const {
		return (double)(x&MASK)/MASK;
	}
	static constexpr Color color(int r, int g, int b, int a) {
		uint32_t res = r;
		res <<= 8;
		res |= g;
		res <<= 8;
		res |= b;
		res <<= 8;
		res |= a;
		return res;
	}
	static const Color BLACK;
};

struct Point {
	int x, y;
	Point(int _x, int _y) : x(_x), y(_y) {}
};

class PathStroke {
public:
	PathStroke() {}
	const std::vector<Point> &points() const {
		return m_points;
	}
	void push_back(Point point) {
		m_points.push_back(point);
	}
	void reserve_points(size_t n) {
		m_points.reserve(n);
	}
private:
	std::vector<Point> m_points;
};

class PenStroke : public PathStroke {
public:
	PenStroke(int w, Color color) : m_width(w), m_color(color) {}
	int width() const {return m_width;}
	Color color() const {return m_color;}
private:
	int m_width;
	Color m_color;
};

class EraserStroke : public PathStroke {
public:
	EraserStroke(int w) : m_width(w) {}
	int width() const {return m_width;}
private:
	int m_width;
};

// This is just a temporary stroke that isn't saved or exported.
class LaserPointerStroke : public PathStroke {
public:
	LaserPointerStroke(int w, Color color) : m_width(w), m_color(color) {}
	int width() const {return m_width;}
	Color color() const {return m_color;}
private:
	int m_width;
	Color m_color;
};

typedef std::variant<PenStroke*, EraserStroke*> ptr_Stroke;
typedef variant_unique<PenStroke, EraserStroke> unique_ptr_Stroke;
typedef std::variant<PenStroke*, EraserStroke*, LaserPointerStroke*> ptr_temp_Stroke;
typedef variant_unique<PenStroke, EraserStroke, LaserPointerStroke> unique_ptr_temp_Stroke;

struct stroke_unique_to_ptr_helper {
	typedef unique_ptr_Stroke in_type;
	typedef ptr_Stroke out_type;
	out_type operator()(const in_type &p) {
		return get(p);
	}
};

class NormalLayer : public QObject {
	Q_OBJECT
public:
	NormalLayer() {}
	explicit NormalLayer(const NormalLayer& a);
	auto strokes() const {
		return VectorView<stroke_unique_to_ptr_helper>(m_strokes);
	}
	void add_stroke(unique_ptr_Stroke stroke) {
		m_strokes.emplace_back(std::move(stroke));
		emit stroke_added();
	}
	unique_ptr_Stroke delete_stroke() {
		emit stroke_deleting();
		unique_ptr_Stroke stroke = std::move(m_strokes.back());
		m_strokes.pop_back();
		return stroke;
	}
	void reserve_strokes(size_t n) {
		m_strokes.reserve(n);
	}
signals:
	void stroke_added(); // Emitted after adding a stroke in the end.
	void stroke_deleting(); // Emitted before deleting the last stroke.
private:
	std::vector<unique_ptr_Stroke> m_strokes;
};

class Page : public QObject {
	Q_OBJECT
public:
	Page(int w, int h) : m_width(w), m_height(h) {}
	explicit Page(const Page& a);
	int width() const {
		return m_width;
	}
	int height() const {
		return m_height;
	}
	auto layers() const {
		return vector_unique_to_pointer(m_layers);
	}
	void add_layer(int at, std::unique_ptr<NormalLayer> layer) {
		m_layers.insert(m_layers.begin() + at, std::move(layer));
		emit layer_added(at);
	}
	void add_layer(int at) {
		add_layer(at, std::make_unique<NormalLayer>());
	}
signals:
	void layer_added(int index);
	void layer_deleted(int index);
private:
	int m_width, m_height;
	std::vector<std::unique_ptr<NormalLayer> > m_layers;
};

class Document : public QObject
{
	Q_OBJECT
public:
	Document() {}
	explicit Document(const Document& a);
	auto pages() const {
		return vector_unique_to_pointer(m_pages);
	}
	void add_page(int at, std::unique_ptr<Page> page) {
		assert(0 <= at && at <= (int)m_pages.size());
		m_pages.insert(m_pages.begin() + at, std::move(page));
		emit page_added(at);
	}
	// Delete and return a page. This returns the page's ownership to the caller. (So if the caller doesn't use the result, the page will be deleted from memory.) We return the deleted page so that we can undo deletion.
	std::unique_ptr<Page> delete_page(int index) {
		assert(0 <= index && index < (int)m_pages.size());
		std::unique_ptr<Page> page = std::move(m_pages[index]);
		m_pages.erase(m_pages.begin() + index);
		emit page_deleted(index);
		return page;
	}
signals:
	void page_added(int index);
	void page_deleted(int index);
private:
	std::vector<std::unique_ptr<Page> > m_pages;
public:
	static std::unique_ptr<Document> concatenate(const std::vector<std::unique_ptr<Document> > &in_docs);
};

#endif // DOCUMENT_H
