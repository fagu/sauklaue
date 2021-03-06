#ifndef PAGEWIDGET_H
#define PAGEWIDGET_H

#include "all-types.h"

#include <functional>
#include <optional>

#include <QWidget>

class ToolState;
class PictureTransformation;

class StrokeCreator {
public:
	StrokeCreator(unique_ptr_Stroke stroke, std::function<void(unique_ptr_Stroke)> committer, DrawingLayerPicture* pic);
	~StrokeCreator();  // Resets (deletes) the current_stroke in pic (which has to equal m_stroke).
	void commit();  // Commits the stroke using the given committer. (This should add the stroke to the picture.)
	void add_point(Point p);
	DrawingLayerPicture* pic() const {
		return m_pic;
	}

private:
	unique_ptr_Stroke m_stroke;
	std::function<void(unique_ptr_Stroke)> m_committer;
	DrawingLayerPicture* m_pic;
};

class ToolCursor : public QObject {
	Q_OBJECT
public:
	ToolCursor(QPointF pos, const PictureTransformation* transformation) :
	    m_pos(pos), m_transformation(transformation) {
	}
	~ToolCursor() {
		emit update(m_rect);
	}
	// Call this after connecting to the update signal.
	void init() {
		m_rect = rect();
		emit update(m_rect);
	}

private:
	// Updates the rectangle and notifies the signal.
	void setRect() {
		emit update(m_rect);
		m_rect = rect();
		emit update(m_rect);
	}

public:
	// Move the cursor to the given point (in widget coordinates).
	void move(QPointF pos) {
		m_pos = pos;
		setRect();
	}
	QPointF pos() const {
		return m_pos;
	}

protected:
	const PictureTransformation* transformation() const {
		return m_transformation;
	}

public:
	// A bounding rectangle for the cursor.
	virtual QRect rect() const = 0;
	// Draw the cursor.
	virtual void paint(QPainter& painter) const = 0;
signals:
	// Notification that the given rectangle needs to be re-drawn.
	void update(const QRect& rect);

private:
	QPointF m_pos;
	QRect m_rect;
	const PictureTransformation* m_transformation;
};

class EraserCursor : public ToolCursor {
public:
	EraserCursor(QPointF pos, const PictureTransformation* transformation, int width) :
	    ToolCursor(pos, transformation), m_width(width) {
	}

public:
	QRect rect() const override;
	void paint(QPainter& painter) const override;

private:
	double radius() const;
	int m_width;
};

class PageWidget : public QWidget {
	Q_OBJECT

public:
	explicit PageWidget(ToolState* toolState);

	void setPage(SPage* page);

public:
	// The rectangle encompassing this page in global screen coordinates. This is used for tablet mapping.
	QRectF minimum_rect_in_pixels();
signals:
	// Notification that the minimum_rect_in_pixels has changed.
	void update_minimum_rect_in_pixels();

public:
	void focusPage();
	void unfocusPage();
signals:
	void focus();  // Signals that this widget would like to request focus.

protected:
	void paintEvent(QPaintEvent* event) override;

	void resizeEvent(QResizeEvent* event) override;
	void moveEvent(QMoveEvent* event) override;

	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

	void tabletEvent(QTabletEvent* event) override;

private:
	void update_page(const QRect& rect);
	void removing_layer_picture(ptr_LayerPicture layer_picture);

	void setupPicture();

	enum struct StrokeType {
		Pen,
		Eraser,
		LaserPointer
	};
	void start_path(QPointF p, StrokeType type);
	void continue_path(QPointF p);
	void finish_path();

	void set_tool_cursor(std::unique_ptr<ToolCursor> tool_cursor);
	void move_tool_cursor(QPointF pos);

private:
	ToolState* m_tool_state;
	// The following are equivalent:
	//  a) !m_page
	//  b) !m_page_picture
	// They imply:
	//  c) !has_focus
	//  d) !m_current_stroke
	//  e) !m_tool_cursor
	SPage* m_page = nullptr;
	std::unique_ptr<PagePicture> m_page_picture;
	bool has_focus = false;
	std::optional<StrokeCreator> m_current_stroke;
	std::unique_ptr<ToolCursor> m_tool_cursor;
};

#endif  // PAGEWIDGET_H
