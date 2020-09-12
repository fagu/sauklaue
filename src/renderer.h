#ifndef RENDERER_H
#define RENDERER_H

#include "document.h"

#include <QObject>
#include <QImage>

#include <cairomm/context.h>
#include <cairomm/surface.h>

class PagePicture;

class LayerPicture : public QObject {
	Q_OBJECT
public:
	virtual QImage img() const = 0;
signals:
	void update(const QRect& rect);
};

class NormalLayerPicture : public LayerPicture {
	Q_OBJECT
public:
	NormalLayerPicture(NormalLayer *layer, PagePicture *page_picture);
	
	Cairo::RefPtr<Cairo::ImageSurface> cairo_surface;
	Cairo::RefPtr<Cairo::Context> cr;
	
	QImage img() const override {
		cairo_surface->flush();
		return QImage((const uchar*)cairo_surface->get_data(), cairo_surface->get_width(), cairo_surface->get_height(), QImage::Format_ARGB32_Premultiplied);
	}
	
private slots:
	void stroke_added(ptr_Stroke stroke);
	void stroke_deleted(ptr_Stroke stroke);
	
private:
	NormalLayer *m_layer;
	PagePicture *m_page_picture;
	
	void set_transparent();
	void setup_stroke(ptr_Stroke stroke);
	void draw_stroke(ptr_Stroke stroke);
	QRect stroke_extents(); // Bounding rectangle of the current path in output coordinates
public:
	void draw_line(Point a, Point b, ptr_Stroke stroke);
};

class PDFLayerPicture : public LayerPicture {
	Q_OBJECT
public:
	PDFLayerPicture(PDFLayer *layer, PagePicture *page_picture);
	QImage img() const override {
		return m_img;
	}
private:
	PDFLayer *m_layer;
	PagePicture *m_page_picture;
	QImage m_img;
};

typedef std::variant<NormalLayerPicture*, PDFLayerPicture*> ptr_LayerPicture;
typedef variant_unique<NormalLayerPicture, PDFLayerPicture> unique_ptr_LayerPicture;

struct layer_picture_unique_to_ptr_helper {
	typedef unique_ptr_LayerPicture in_type;
	typedef ptr_LayerPicture out_type;
	out_type operator()(const in_type &p) {
		return get(p);
	}
};

class PagePicture : public QObject {
	Q_OBJECT
public:
	explicit PagePicture(SPage *_page, int _width, int _height);
	
	Cairo::Matrix page2widget, widget2page;
	Cairo::Matrix page2image, image2page;
	SPage *page;
	int width, height;
	int page_width, page_height;
	int dx, dy;
	auto layers() const {
		return VectorView<layer_picture_unique_to_ptr_helper>(m_layers);
	}
	NormalLayerPicture* temporary_layer() const {
		return m_temporary_layer.get();
	}
private:
	std::vector<unique_ptr_LayerPicture> m_layers;
	std::unique_ptr<NormalLayerPicture> m_temporary_layer;
	
private slots:
	void register_layer(int index);
	void unregister_layer(int index);
	
	void update_layer(const QRect& rect);
	
signals:
	void update(const QRect& rect);
};


class PDFExporter {
public:
	static void save(Document *doc, const std::string &file_name);
};

#endif // RENDERER_H
