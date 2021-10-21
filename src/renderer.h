#ifndef RENDERER_H
#define RENDERER_H

#include "all-types.h"

#include <optional>

#include <QObject>
#include <QImage>

#include <cairomm/context.h>
#include <cairomm/surface.h>

// RAII for the cairo context: Saves the current state on construction and restores it on deletion.
class CairoGroup {
public:
	CairoGroup(Cairo::RefPtr<Cairo::Context> cr) :
	    _cr(cr) {
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

// For each layer, we generate an image whose size agrees with the size of the page on screen.
// TODO Support zooming. Note that when zooming in, we should only generate an image for part of the page.

class PictureTransformation {
public:
	PictureTransformation(SPage* _page, int widget_width, int widget_height);
	double unit2pixel;  // How many pixels correspond to 1 unit.
	// The coordinate (0,0) on the page is assumed to be the coordinate (0,0) in the image. (This would need to be changed to support zooming.)
	QRect image_rect;  // The rectangle occupied by the image on screen.
	QSize image_size;  // Size of the image on screen. ( = image_rect.size())
	QPoint topLeft;  // Top left corner of the image in the widget. Add this to turn a point in the image into a point in the widget. ( = image_rect.topLeft())
	Point widget2page(QPointF point) const {
		point -= topLeft;
		double x = point.x() / unit2pixel, y = point.y() / unit2pixel;
		return Point(x, y);
	}
};

class LayerPicture : public QObject {
	Q_OBJECT
public:
	LayerPicture(const PictureTransformation& transformation) :
	    m_transformation(transformation) {
	}
	virtual QImage img() const = 0;
	const PictureTransformation& transformation() const {
		return m_transformation;
	}
signals:
	// Notification that the given rectangle in the image was repainted.
	void update(const QRect& rect);

private:
	const PictureTransformation& m_transformation;
};

class Renderer {
public:
	Renderer(const PictureTransformation& transformation);

	// Resets every pixel to transparent.
	// If a rectangle is given, only pixels inside the rectangle (in pixel coordinates) are reset.
	void set_transparent(std::optional<QRect> rect = std::nullopt);
	QImage img() const;
	// Copies the contents of the given rectangle from another cairo image.
	void copy_from(const Renderer& other_renderer, std::optional<QRect> rect = std::nullopt);
	QRect draw_stroke(ptr_Stroke stroke, std::optional<QRect> clip_rect = std::nullopt);
	QRect stroke_extents(ptr_Stroke stroke);

private:
	const PictureTransformation& m_transformation;
	Cairo::RefPtr<Cairo::ImageSurface> cairo_surface;
	Cairo::RefPtr<Cairo::Context> cr;
	void setup_stroke(ptr_Stroke stroke);
	QRect current_stroke_extents();  // Bounding rectangle of the current path in output coordinates
};

class DrawingLayerPicture : public LayerPicture {
	Q_OBJECT
public:
	DrawingLayerPicture(std::variant<NormalLayer*, TemporaryLayer*> layer, const PictureTransformation& transformation);

	QImage img() const override {
		return all_strokes.img();
	}

	void set_current_stroke(ptr_Stroke current_stroke);
	void reset_current_stroke();

private:
	// Redraw all strokes in the given rectangle.
	void redraw(std::optional<QRect> rect = std::nullopt);
	// Redraw only the current stroke in the given rectangle.
	void redraw_current(std::optional<QRect> rect = std::nullopt);

	void stroke_added(ptr_Stroke stroke);
	void stroke_deleted(ptr_Stroke stroke);

	// A picture of all strokes except the current one.
	Renderer committed_strokes;
	// A picture of all strokes including the current one.
	Renderer all_strokes;

	std::variant<NormalLayer*, TemporaryLayer*> m_layer;
	std::optional<ptr_Stroke> m_current_stroke;  // This is drawn after all the strokes in m_layer. When the stroke is extended, you must call draw_line. When it is finished, add it to m_layer. The current_stroke is then automatically reset to nullptr.
	void draw_strokes();

public:
	void draw_line(Point a, Point b, ptr_Stroke stroke);
};

class PDFLayerPicture : public LayerPicture {
	Q_OBJECT
public:
	PDFLayerPicture(PDFLayer* layer, const PictureTransformation& transformation);
	QImage img() const override {
		cairo_surface->flush();
		return QImage((const uchar*)cairo_surface->get_data(), cairo_surface->get_width(), cairo_surface->get_height(), QImage::Format_ARGB32_Premultiplied);
	}

private:
	void redraw();

	PDFLayer* m_layer;
	Cairo::RefPtr<Cairo::ImageSurface> cairo_surface;
};

class PagePicture : public QObject {
	Q_OBJECT
public:
	explicit PagePicture(SPage* _page, int _width, int _height);

	SPage* page;
	auto layers() const {
		return VectorView<layer_picture_unique_to_ptr_helper>(m_layers);
	}
	DrawingLayerPicture* temporary_layer() const {
		return m_temporary_layer.get();
	}
	const PictureTransformation& transformation() const {
		return m_transformation;
	}

private:
	PictureTransformation m_transformation;
	std::vector<unique_ptr_LayerPicture> m_layers;
	std::unique_ptr<DrawingLayerPicture> m_temporary_layer;

	void register_layer(int index);
	void unregister_layer(int index);

	void update_layer(const QRect& rect);

signals:
	// Notification that the given rectangle in the image was repainted.
	void update(const QRect& rect);
	void removing_layer(ptr_LayerPicture layer);  // Emitted just before removing the given layer picture.
};

class PDFExporter {
public:
	static void save(Document* doc, const std::string& file_name);
};

#endif  // RENDERER_H
