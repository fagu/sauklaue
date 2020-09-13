#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "util.h"

#include <list>
#include <memory>
#include <variant>
#include <vector>

#include <QColor>
#include <QString>
#include <QDataStream>
#include <QTimer>
#include <QDebug>

#include <poppler-qt5.h>

class Color {
public:
	constexpr Color(uint32_t _x) : x(_x) {}
	Color(QColor c) : Color(color(c.red(),c.green(),c.blue(),c.alpha())) {}
	
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

const double POINT_TO_UNIT = 1000;
const double UNIT_TO_POINT = 1/POINT_TO_UNIT;
const double INCH_TO_UNIT = 72*POINT_TO_UNIT;
const double UNIT_TO_INCH = 1/INCH_TO_UNIT;

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

typedef std::variant<PenStroke*, EraserStroke*> ptr_Stroke;
typedef variant_unique<PenStroke, EraserStroke> unique_ptr_Stroke;

struct stroke_unique_to_ptr_helper {
	typedef unique_ptr_Stroke in_type;
	typedef ptr_Stroke out_type;
	out_type operator()(const in_type &p) {
		return get(p);
	}
};

class NormalLayer;
class TemporaryStroke : public QObject {
	Q_OBJECT
public:
	TemporaryStroke(NormalLayer* layer, unique_ptr_Stroke stroke);
	// Call this method exactly once!
	// This sets the iterator (which doesn't exist at the time we construct the TemporaryStroke object).
	// It then starts the timer.
	void start(std::list<TemporaryStroke>::iterator it, int timeout);
	ptr_Stroke stroke() const {
		return get(m_stroke);
	}
private:
	NormalLayer *m_layer;
	unique_ptr_Stroke m_stroke;
	QTimer *m_timer;
	std::list<TemporaryStroke>::iterator m_it;
private slots:
	void timeout();
};

struct temporary_stroke_to_ptr {
	typedef TemporaryStroke in_type;
	typedef ptr_Stroke out_type;
	out_type operator()(const in_type &p) {
		return p.stroke();
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
		emit stroke_added(get(m_strokes.back()));
	}
	unique_ptr_Stroke delete_stroke() {
		unique_ptr_Stroke stroke = std::move(m_strokes.back());
		m_strokes.pop_back();
		emit stroke_deleted(get(stroke));
		return stroke;
	}
	void reserve_strokes(size_t n) {
		m_strokes.reserve(n);
	}
	auto temporary_strokes() const {
		return ListView<temporary_stroke_to_ptr>(m_temporary_strokes);
	}
	void add_temporary_stroke(unique_ptr_Stroke stroke, int timeout) {
		std::list<TemporaryStroke>::iterator it = m_temporary_strokes.emplace(m_temporary_strokes.end(), this, std::move(stroke));
		it->start(it, timeout);
		emit stroke_added(it->stroke()); // We can't call get(stroke) here, because stroke has been moved.
	}
	void delete_temporary_stroke(std::list<TemporaryStroke>::iterator it, unique_ptr_Stroke stroke) {
		m_temporary_strokes.erase(it);
		emit stroke_deleted(get(stroke));
		// At this point, the stroke is destructed.
	}
signals:
	void stroke_added(ptr_Stroke stroke); // Emitted after adding a stroke (permanent or temporary).
	void stroke_deleted(ptr_Stroke stroke); // Emitted after deleting a stroke (permanent or temporary). Of course, the stroke is not destructed before emitting this signal.
private:
	std::vector<unique_ptr_Stroke> m_strokes;
	// Any temporary stroke comes with a timer that deletes the stroke after a certain amount of time.
	// Temporary strokes are not saved or exported to PDF files. They do not participate in the undo mechanism.
	std::list<TemporaryStroke> m_temporary_strokes;
	// TODO The order in which permanent and temporary strokes are drawn is not well-defined. (???)
};

class EmbeddedPDF {
public:
	EmbeddedPDF(const QString &name, const QByteArray &contents);
	QString name() const {
		return m_name;
	}
	QByteArray contents() const {
		return m_contents;
	}
	Poppler::Document* document() const {
		return m_document.get();
	}
	auto pages() const {
		return vector_unique_to_pointer(m_pages);
	}
private:
	QString m_name;
	QByteArray m_contents;
	std::unique_ptr<Poppler::Document> m_document;
	std::vector<std::unique_ptr<Poppler::Page> > m_pages; // Destructed before m_document
};

class PDFLayer : public QObject {
	Q_OBJECT
public:
	PDFLayer(EmbeddedPDF *pdf, int page_number) : m_pdf(pdf), m_page_number(page_number) {
		assert(0 <= page_number && page_number < (int)m_pdf->pages().size());
	}
	EmbeddedPDF* pdf() const {
		return m_pdf;
	}
	int page_number() const {
		return m_page_number;
	}
	Poppler::Page* page() const {
		return m_pdf->pages()[m_page_number];
	}
private:
	EmbeddedPDF *m_pdf;
	int m_page_number;
};

typedef std::variant<NormalLayer*, PDFLayer*> ptr_Layer;
typedef variant_unique<NormalLayer, PDFLayer> unique_ptr_Layer;

struct layer_unique_to_ptr_helper {
	typedef unique_ptr_Layer in_type;
	typedef ptr_Layer out_type;
	out_type operator()(const in_type &p) {
		return get(p);
	}
};

// We call this class SPage instead of Page to avoid a collision with the class Page in the poppler library. Ridiculously, this name clash causes the destructor of our Page to be called instead of the destructor of poppler's Page, so the program crashes.^^
class SPage : public QObject {
	Q_OBJECT
public:
	SPage(int w, int h) : m_width(w), m_height(h), m_temporary_layer(std::make_unique<NormalLayer>()) {
		assert(m_width >= 1 && m_height >= 1);
	}
// 	explicit Page(const Page& a); // Note: The copy constructor does not copy the temporary layer!
	int width() const {
		return m_width;
	}
	int height() const {
		return m_height;
	}
	auto layers() const {
		return VectorView<layer_unique_to_ptr_helper>(m_layers);
	}
	void add_layer(int at, unique_ptr_Layer layer) {
		m_layers.insert(m_layers.begin() + at, std::move(layer));
		emit layer_added(at);
	}
	void add_layer(int at) {
		add_layer(at, std::make_unique<NormalLayer>());
	}
	NormalLayer* temporary_layer() const {
		return m_temporary_layer.get();
	}
signals:
	void layer_added(int index);
	void layer_deleted(int index);
private:
	int m_width, m_height;
	std::vector<unique_ptr_Layer> m_layers;
	std::unique_ptr<NormalLayer> m_temporary_layer;
};

class Document : public QObject
{
	Q_OBJECT
public:
	Document() {}
// 	explicit Document(const Document& a);
	auto pages() const {
		return vector_unique_to_pointer(m_pages);
	}
	void add_pages(int at, std::vector<std::unique_ptr<SPage> > pages) {
		assert(0 <= at && at <= (int)m_pages.size());
		assert(pages.size() > 0);
		m_pages.insert(m_pages.begin() + at, std::make_move_iterator(pages.begin()), std::make_move_iterator(pages.end()));
		emit pages_added(at, pages.size());
	}
	void add_page(int at, std::unique_ptr<SPage> page) {
		add_pages(at, move_into_vector(std::move(page)));
	}
	// Delete and return a page. This returns the page's ownership to the caller. (So if the caller doesn't use the result, the page will be deleted from memory.) We return the deleted page so that we can undo deletion.
	std::vector<std::unique_ptr<SPage> > delete_pages(int first_page, int number_of_pages) {
		assert(0 <= first_page && 0 <= number_of_pages && first_page + number_of_pages <= (int)m_pages.size());
		std::vector<std::unique_ptr<SPage> > pages(std::make_move_iterator(m_pages.begin() + first_page), std::make_move_iterator(m_pages.begin() + first_page + number_of_pages));
		m_pages.erase(m_pages.begin() + first_page, m_pages.begin() + first_page + number_of_pages);
		emit pages_deleted(first_page, number_of_pages);
		return pages;
	}
	auto embedded_pdfs() const {
		return list_unique_to_pointer(m_embedded_pdfs);
	}
	std::list<std::unique_ptr<EmbeddedPDF> >::iterator add_embedded_pdf(std::unique_ptr<EmbeddedPDF> pdf) {
		return m_embedded_pdfs.insert(m_embedded_pdfs.end(), std::move(pdf));
	}
	std::unique_ptr<EmbeddedPDF> delete_embedded_pdf(std::list<std::unique_ptr<EmbeddedPDF> >::iterator it) {
		std::unique_ptr<EmbeddedPDF> pdf = std::move(*it);
		m_embedded_pdfs.erase(it);
		return pdf;
	}
signals:
	void pages_added(int first_page, int number_of_pages);
	void pages_deleted(int first_page, int number_of_pages);
private:
	std::vector<std::unique_ptr<SPage> > m_pages; // TODO Use a data structure that is better suited for insertion?
	std::list<std::unique_ptr<EmbeddedPDF> > m_embedded_pdfs;
// public:
// 	static std::unique_ptr<Document> concatenate(const std::vector<std::unique_ptr<Document> > &in_docs);
};

#endif // DOCUMENT_H
