#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <memory>
#include <vector>

#include <QString>
#include <QDataStream>

class Color {
public:
	Color(uint32_t _x) : x(_x) {}
	
	uint32_t x;
	
	static constexpr uint32_t MASK = 255;
	
	double r() const {
		return (double)((x>>24)&MASK)/(1<<8);
	}
	double g() const {
		return (double)((x>>16)&MASK)/(1<<8);
	}
	double b() const {
		return (double)((x>>8)&MASK)/(1<<8);
	}
	double a() const {
		return (double)(x&MASK)/(1<<8);
	}
	static constexpr uint32_t color(int r, int g, int b, int a) {
		uint32_t res = r;
		res >>= 8;
		res |= g;
		res >>= 8;
		res |= b;
		res >>= 8;
		res |= a;
		return res;
	}
	static const uint32_t BLACK;
	static const uint32_t TRANSPARENT;
};

struct Point {
	int x, y;
	Point(int _x, int _y) : x(_x), y(_y) {}
};

class Stroke {
public:
	Stroke(int w, uint32_t color) : m_width(w), m_color(color) {}
	const std::vector<Point> &points() const;
	Point push_back(Point point);
	int width() const {return m_width;}
	uint32_t color() const {return m_color;}
private:
	std::vector<Point> m_points;
	int m_width;
	uint32_t m_color;
};

class NormalLayer : public QObject {
	Q_OBJECT
public:
	NormalLayer();
	const std::vector<std::unique_ptr<Stroke> > & strokes();
	void add_stroke(std::unique_ptr<Stroke> stroke)
	{
		m_strokes.emplace_back(std::move(stroke));
		emit stroke_added();
	}
	std::unique_ptr<Stroke> delete_stroke()
	{
		emit stroke_deleting();
		std::unique_ptr<Stroke> stroke = std::move(m_strokes.back());
		m_strokes.pop_back();
		return stroke;
	}
signals:
	void stroke_added(); // Emitted after adding a stroke in the end.
	void stroke_deleting(); // Emitted before deleting the last stroke.
private:
	std::vector<std::unique_ptr<Stroke> > m_strokes;
};

class Page : public QObject {
	Q_OBJECT
public:
	Page(int w, int h);
	int width();
	int height();
	const std::vector<std::unique_ptr<NormalLayer> > & layers() const
	{
		return m_layers;
	}
	void add_layer(int at, std::unique_ptr<NormalLayer> layer)
	{
		m_layers.insert(m_layers.begin() + at, std::move(layer));
		emit layer_added(at);
	}
	void add_layer(int at)
	{
		add_layer(at, std::make_unique<NormalLayer>());
	}
	NormalLayer * layer(int index) {
		assert(0 <= index && index < (int)m_layers.size());
		return m_layers[index].get();
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
	Document();
	void add_page(int at, std::unique_ptr<Page> page);
	std::unique_ptr<Page> delete_page(int index); // Delete and return a page. This returns the page's ownership to the caller. (So if the caller doesn't use the result, the page will be deleted from memory.) We return the deleted page so that we can undo deletion.
	Page* page(int index);
	int number_of_pages();
	void save(QDataStream &stream);
signals:
	void page_added(int index);
	void page_deleted(int index);
private:
	std::vector<std::unique_ptr<Page> > pages;
public:
	static std::unique_ptr<Document> load(QDataStream &stream);
};

#endif // DOCUMENT_H
