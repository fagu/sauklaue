#ifndef RENDERER_H
#define RENDERER_H

#include "document.h"

#include <QObject>

#include <cairomm/context.h>
#include <cairomm/surface.h>

class PagePicture;

class LayerPicture : public QObject {
	Q_OBJECT
public:
	LayerPicture(NormalLayer *layer, PagePicture *page_picture);
	
	Cairo::RefPtr<Cairo::ImageSurface> cairo_surface;
	Cairo::RefPtr<Cairo::Context> cr;
	
private slots:
	void stroke_added(ptr_Stroke stroke);
	void stroke_deleted(ptr_Stroke stroke);
	
signals:
	void update(const QRect& rect);
	
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

class PagePicture : public QObject {
	Q_OBJECT
public:
	explicit PagePicture(Page *_page, int _width, int _height);
	
	Cairo::Matrix page2widget, widget2page;
	Page *page;
	int width;
	int height;
	auto layers() const {
		return vector_unique_to_pointer(m_layers);
	}
	LayerPicture* temporary_layer() const {
		return m_temporary_layer.get();
	}
private:
	std::vector<std::unique_ptr<LayerPicture> > m_layers;
	std::unique_ptr<LayerPicture> m_temporary_layer;
	
private slots:
	void register_layer(int index);
	void unregister_layer(int index);
	
	void update_layer(const QRect& rect);
	
signals:
	void update(const QRect& rect);
};


class PDFExporter {
public:
	static void save(Document *doc, const std::string &file_name, bool simplistic);
};

#endif // RENDERER_H
