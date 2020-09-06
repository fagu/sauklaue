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
private:
	PagePicture *m_page_picture;
	NormalLayer *m_layer;
};

class PagePicture : public QObject {
	Q_OBJECT
public:
	explicit PagePicture(Page *page);
	
private:
	Page *m_page;
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
	
private:
	void setupCairo();
	Point add_to_path(Point p);
	void finish_path();
	void draw_line(Point a, Point b);
	void bounding_rect_helper(double x, double y, double &minx, double &maxx, double &miny, double &maxy);
	void updatePageRect(double x1, double y1, double x2, double y2);
	
private slots:
	void register_layer(int index);
	void unregister_layer(int index);
	void stroke_added();
	void stroke_deleted();
	
private:
	sauklaue *m_view;
	Cairo::RefPtr<Cairo::ImageSurface> cairo_surface;
	Cairo::RefPtr<Cairo::Context> cr;
	Cairo::Matrix page2widget, widget2page;
	bool pressed = false;
	Page *m_page = nullptr;
	std::unique_ptr<Stroke> m_current_path;
};

#endif // PAGEWIDGET_H
