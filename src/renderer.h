#ifndef RENDERER_H
#define RENDERER_H

#include "document.h"

#include <QObject>
#include <QImage>

#include <cairomm/context.h>
#include <cairomm/surface.h>

class PagePicture;

// For each layer, we generate an image whose size agrees with the size of the page on screen.
// TODO Support zooming. Note that when zooming in, we should only generate an image for part of the page.

class PictureTransformation {
public:
	PictureTransformation(SPage *_page, int widget_width, int widget_height);
	double unit2pixel; // How many pixels correspond to 1 unit.
	QRect image_rect; // The rectangle occupied by the image on screen.
	QSize image_size; // Size of the image on screen. ( = image_rect.size())
	QPoint topLeft; // Top left corner of the image in the widget. Add this to turn a point in the image into a point in the widget. ( = image_rect.topLeft())
	QSize widget_size; // Size of the widget displaying the page. This could includes some space around the image.
	Cairo::Matrix page2image, image2page; // Transformation sending a point on the page to a point in the image.
	Point widget2page(QPointF point) const {
		point -= topLeft;
		double x = point.x(), y = point.y();
		image2page.transform_point(x,y);
		return Point(x,y);
	}
	Cairo::Matrix page2widget;
};

class LayerPicture : public QObject {
	Q_OBJECT
public:
	LayerPicture(const PictureTransformation& transformation) : m_transformation(transformation) {}
	virtual QImage img() const = 0;
	const PictureTransformation& transformation() const {
		return m_transformation;
	}
signals:
	// Notification that the given rectangle in the image was repainted.
	void update(const QRect& rect);
private:
	const PictureTransformation &m_transformation;
};

class NormalLayerPicture : public LayerPicture {
	Q_OBJECT
public:
	NormalLayerPicture(NormalLayer *layer, const PictureTransformation &transformation);
	
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
	PDFLayerPicture(PDFLayer *layer, const PictureTransformation &transformation);
	QImage img() const override {
		return m_img;
	}
private:
	PDFLayer *m_layer;
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
	
	SPage *page;
	auto layers() const {
		return VectorView<layer_picture_unique_to_ptr_helper>(m_layers);
	}
	NormalLayerPicture* temporary_layer() const {
		return m_temporary_layer.get();
	}
	const PictureTransformation& transformation() const {
		return m_transformation;
	}
private:
	PictureTransformation m_transformation;
	std::vector<unique_ptr_LayerPicture> m_layers;
	std::unique_ptr<NormalLayerPicture> m_temporary_layer;
	
private slots:
	void register_layer(int index);
	void unregister_layer(int index);
	
	void update_layer(const QRect& rect);
	
signals:
	// Notification that the given rectangle in the image was repainted.
	void update(const QRect& rect);
};


class PDFExporter {
public:
	static void save(Document *doc, const std::string &file_name);
};

#endif // RENDERER_H
