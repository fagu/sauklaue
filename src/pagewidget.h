#ifndef PAGEWIDGET_H
#define PAGEWIDGET_H

#include "document.h"
#include "renderer.h"

#include <functional>
#include <memory>

#include <QWidget>

#include <cairomm/context.h>
#include <cairomm/surface.h>

class MainWindow;

class StrokeCreator {
public:
	StrokeCreator(unique_ptr_Stroke stroke, std::function<void(unique_ptr_Stroke)> committer, DrawingLayerPicture *pic);
	~StrokeCreator(); // Resets (deletes) the current_stroke in pic (which has to equal m_stroke).
	void commit(); // Commits the stroke using the given committer. (This should add the stroke to the picture.)
	void add_point(Point p);
	DrawingLayerPicture *pic() const {
		return m_pic;
	}
private:
	unique_ptr_Stroke m_stroke;
	std::function<void(unique_ptr_Stroke)> m_committer;
	DrawingLayerPicture *m_pic;
};

class PageWidget : public QWidget
{
	Q_OBJECT

public:
	explicit PageWidget(MainWindow *view);
	
	void setPage(SPage *page);
	
private:
	Cairo::Matrix page_to_pixels();
public:
	QRectF minimum_rect_in_pixels();
	void update_tablet_map();
	
public:
	void focusPage();
	void unfocusPage();
signals:
	void focus(); // Signals that this widget would like to request focus.
	
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
	void removing_layer_picture(ptr_LayerPicture layer_picture);
	
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
	MainWindow *m_view;
	// The following are equivalent:
	//  a) !m_page
	//  b) !m_page_picture
	// They imply:
	//  c) !has_focus
	//  d) !m_current_stroke
	SPage *m_page = nullptr;
	std::unique_ptr<PagePicture> m_page_picture;
	bool has_focus = false;
	std::optional<StrokeCreator> m_current_stroke;
};

#endif // PAGEWIDGET_H
