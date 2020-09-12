#ifndef PAGEWIDGET_H
#define PAGEWIDGET_H

#include "document.h"
#include "renderer.h"

#include <memory>

#include <QWidget>

#include <cairomm/context.h>
#include <cairomm/surface.h>

class MainWindow;

struct StrokeCreator {
	unique_ptr_Stroke stroke;
	int timeout; // -1 for permanent strokes, > 0 for temporary strokes.
};

class PageWidget : public QWidget
{
	Q_OBJECT

public:
	explicit PageWidget(MainWindow *view);
	
	void setPage(SPage *page, int index);
	
	Cairo::Matrix tablet_to_reality();
	Cairo::Matrix page_to_pixels();
	QRectF minimum_rect_in_pixels();
	Cairo::Matrix tablet_to_screen();
	void update_tablet_map();
	
	void focusPage();
	void unfocusPage();
	
protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void moveEvent(QMoveEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent * event) override;
	void tabletEvent(QTabletEvent * event) override;
	
private slots:
	void update_page(const QRect& rect);
	
private:
	void setupPicture();
	
	enum struct StrokeType {
		Pen,
		Eraser,
		LaserPointer
	};
	void start_path(QPointF p, StrokeType type);
	void continue_path(QPointF p);
	void finish_path();
	
private:
	bool has_focus = false;
	int page_index;
	MainWindow *m_view;
	SPage *m_page = nullptr;
	std::unique_ptr<PagePicture> m_page_picture;
	std::optional<StrokeCreator> m_current_stroke;
};

#endif // PAGEWIDGET_H
