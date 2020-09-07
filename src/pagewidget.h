#ifndef PAGEWIDGET_H
#define PAGEWIDGET_H

#include "document.h"

#include <memory>

#include <QWidget>

#include <cairomm/context.h>
#include <cairomm/surface.h>

class sauklaue;
class PagePicture;

class LayerPicture : public QObject {
	Q_OBJECT
public:
	explicit LayerPicture(NormalLayer *layer, PagePicture *page_picture);
	
	Cairo::RefPtr<Cairo::ImageSurface> cairo_surface;
	Cairo::RefPtr<Cairo::Context> cr;
	
private slots:
	void stroke_added();
	void stroke_deleting();
	
signals:
	void update(const QRect& rect);
	
private:
	NormalLayer *m_layer;
	PagePicture *m_page_picture;
	
	void setup();
	void draw_path(const std::vector<Point> &points);
	void draw_stroke(int i);
public:
	void draw_line(Point a, Point b, ptr_Stroke stroke);
private:
	void bounding_rect_helper(double x, double y, double &minx, double &maxx, double &miny, double &maxy);
	void updatePageRect(double x1, double y1, double x2, double y2);
};

class PagePicture : public QObject {
	Q_OBJECT
public:
	explicit PagePicture(Page *_page, int _width, int _height);
	
	Cairo::Matrix page2widget, widget2page;
	Page *page;
	int width;
	int height;
	std::vector<std::unique_ptr<LayerPicture> > layers;
	
private slots:
	void register_layer(int index);
	void unregister_layer(int index);
	
	void update_layer(const QRect& rect);
	
signals:
	void update(const QRect& rect);
};

class PageWidget : public QWidget
{
	Q_OBJECT

public:
	explicit PageWidget(sauklaue *view);
	
	void setPage(Page *page);
	
	void update_tablet_map();
	
protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void moveEvent(QMoveEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void tabletEvent(QTabletEvent * event) override;
	
private slots:
	void update_page(const QRect& rect);
	
private:
	enum struct StrokeType {
		Pen,
		Eraser
	};
	void start_path(double x, double y, StrokeType type);
	void continue_path(double x, double y);
	void finish_path();
	
private:
	sauklaue *m_view;
	Page *m_page = nullptr;
	std::unique_ptr<PagePicture> m_page_picture;
	std::optional<unique_ptr_Stroke> m_current_path;
};

#endif // PAGEWIDGET_H
