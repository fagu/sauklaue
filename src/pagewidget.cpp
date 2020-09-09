#include "pagewidget.h"

#include "util.h"
#include "mainwindow.h"
#include "actions.h"
#include "tablet.h"

#include <QtWidgets>


const int DEFAULT_LINE_WIDTH = 1500;
const int DEFAULT_ERASER_WIDTH = 1500*20;

PageWidget::PageWidget(MainWindow* view) :
	QWidget(nullptr),
	m_view(view)
{
	setMinimumSize(20,20);
}

void PageWidget::setPage(Page* page, int index)
{
	if (m_page == page)
		return;
	page_index = index;
	m_page = page;
	m_current_stroke.reset();
	setupPicture();
}

void PageWidget::setupPicture()
{
	if (m_page) {
		m_page_picture = std::make_unique<PagePicture>(m_page, width(), height());
		connect(m_page_picture.get(), &PagePicture::update, this, &PageWidget::update_page);
	} else {
		m_page_picture = nullptr;
	}
	update_tablet_map();
	update();
}

void PageWidget::update_page(const QRect& rect)
{
	update(rect);
}

// Map tablet coordinates in [0,1] x [0,1] to coordinates in real life.
Cairo::Matrix PageWidget::tablet_to_reality()
{
	// Tablet size: 28cm x 15.8cm
	// Rotated 90 degrees clockwise
	return Cairo::scaling_matrix(28, 15.8) * Cairo::rotation_matrix(M_PI/2);
}

Cairo::Matrix PageWidget::page_to_pixels()
{
	assert(m_page_picture);
	// We assume here that mapToGlobal is always a translation.
	// TODO Find out if that assumption is correct.
	QPoint translation = mapToGlobal(QPoint(0,0));
	return m_page_picture->page2widget * Cairo::translation_matrix(translation.x(), translation.y());
}

QRectF PageWidget::minimum_rect_in_pixels()
{
	Cairo::Matrix p2s = page_to_pixels();
	// Compute the bounding rectangle of the page in screen coordinates.
	QRectF rect = bounding_rect(p2s, QRectF(QPointF(0,0), QPointF(m_page->width(), m_page->height())));
	static const int MARGIN = 50;
	return QRectF(QPointF(rect.left()-MARGIN, rect.top()-MARGIN), QPointF(rect.right()+MARGIN, rect.bottom()+MARGIN));
}

Cairo::Matrix PageWidget::tablet_to_screen()
{
	assert(m_page);
	Cairo::Matrix t2pi = tablet_to_reality() * page_to_pixels();
	Cairo::Matrix pi2t = t2pi; pi2t.invert();
	// Compute the minimum bounding rectangle in tablet coordinates.
	QRectF minrect = minimum_rect_in_pixels();
	QRectF minrect2 = bounding_rect(pi2t, minrect);
	double scale = std::max(minrect2.width(), minrect2.height());
	double dx = (minrect2.left()+minrect2.right()-scale)/2, dy = (minrect2.top()+minrect2.bottom()-scale)/2;
	QRect scr = screen()->geometry();
	return Cairo::scaling_matrix(scale, scale) * Cairo::translation_matrix(dx, dy) * t2pi * Cairo::scaling_matrix(1./scr.width(), 1./scr.height());
}

void PageWidget::update_tablet_map()
{
	if (!has_focus) {
// 		m_view->tablet->set_transformation_matrix(Cairo::identity_matrix());
		return;
	}
	assert(m_page_picture);
	m_view->tablet->set_transformation_matrix(tablet_to_screen());
}

void PageWidget::focusPage()
{
	has_focus = true;
	update_tablet_map();
	update();
}

void PageWidget::unfocusPage()
{
	has_focus = false;
	update_tablet_map();
	update();
}

void PageWidget::paintEvent(QPaintEvent* event)
{
	qDebug() << "paint" << event->region();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, false);
	painter.fillRect(0,0,width(),height(), QColorConstants::LightGray);
	if (!m_page_picture) {
		// TODO?
		return;
	}
	double x1 = 0, y1 = 0, x2 = m_page->width(), y2 = m_page->height();
	m_page_picture->page2widget.transform_point(x1, y1);
	m_page_picture->page2widget.transform_point(x2, y2);
	painter.fillRect(x1, y1, x2-x1, y2-y1, QColorConstants::White);
	for (auto layer_picture : m_page_picture->layers()) {
		layer_picture->cairo_surface->flush();
		if (width() != layer_picture->cairo_surface->get_width() || height() != layer_picture->cairo_surface->get_height()) {
			qDebug() << "size has recently changed";
			return;
		}
		QImage img((const uchar*)layer_picture->cairo_surface->get_data(), width(), height(), QImage::Format_ARGB32_Premultiplied);
		painter.drawImage(0, 0, img);
	}
	if (has_focus) {
		painter.setPen(QPen(QColorConstants::Red, 3));
		painter.drawRect(x1, y1, x2-x1, y2-y1);
	}
}

void PageWidget::resizeEvent(QResizeEvent* event)
{
	qDebug() << "resize" << event->size();
	setupPicture();
}

void PageWidget::moveEvent(QMoveEvent* event)
{
	update_tablet_map();
}

void PageWidget::mousePressEvent(QMouseEvent* event)
{
	if (!has_focus)
		return;
	if (event->button() == Qt::LeftButton)
		start_path(event->pos().x(), event->pos().y(), StrokeType::Pen);
	else if (event->button() == Qt::RightButton)
		start_path(event->pos().x(), event->pos().y(), StrokeType::Eraser);
}

void PageWidget::mouseMoveEvent(QMouseEvent* event)
{
	continue_path(event->pos().x(), event->pos().y());
}

void PageWidget::mouseReleaseEvent(QMouseEvent* event)
{
	finish_path();
}

void PageWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (page_index != -1)
		m_view->gotoPage(page_index);
}

void PageWidget::tabletEvent(QTabletEvent* event)
{
	if (!has_focus)
		return;
	if (event->pointerType() == QTabletEvent::Pen || event->pointerType() == QTabletEvent::Eraser) {
		if (event->type() == QEvent::TabletPress) {
			// If we push the right button while touching the surface, the left button is automatically pushed as well.
			if (event->button() == Qt::LeftButton) { // Do not draw if we only press the right button while hovering.
				StrokeType type = StrokeType::Pen;
				if (event->pointerType() == QTabletEvent::Eraser || (event->buttons() & Qt::RightButton))
					type = StrokeType::Eraser;
				start_path(event->posF().x(), event->posF().y(), type);
			} else if (event->button() == Qt::RightButton) {
				setCursor(Qt::CrossCursor);
			}
			// If we push the right button while hovering, we don't want to generate a mouse event (which would start erasing).
			// We therefore accept the event no matter which button is pressed.
			event->accept();
		} else if (event->type() == QEvent::TabletMove) {
			continue_path(event->posF().x(), event->posF().y());
			event->accept();
		} else if (event->type() == QEvent::TabletRelease) {
			finish_path();
			if (event->button() == Qt::RightButton)
				unsetCursor();
			event->accept();
		}
	}
}

void PageWidget::start_path(double x, double y, StrokeType type)
{
	if (!m_page_picture)
		return;
	m_page_picture->widget2page.transform_point(x, y);
	if (!m_current_stroke) {
		Point p(x,y);
		if (type == StrokeType::Pen) {
			m_current_stroke = std::make_unique<PenStroke>(DEFAULT_LINE_WIDTH, Color::BLACK);
		} else if (type == StrokeType::Eraser) {
			m_current_stroke = std::make_unique<EraserStroke>(DEFAULT_ERASER_WIDTH);
		}
		std::visit(overloaded {
			[&](std::variant<PenStroke*,EraserStroke*> st) {
				PathStroke* pst = convert_variant<PathStroke*>(st);
				pst->push_back(p);
				assert(m_page_picture->layers().size() == 1);
				m_page_picture->layers()[0]->draw_line(p, p, st);
			}
		}, get(m_current_stroke.value()));
	}
}

void PageWidget::continue_path(double x, double y)
{
	if (!m_current_stroke)
		return;
	m_page_picture->widget2page.transform_point(x, y);
	if (m_current_stroke) {
		Point p(x,y);
		std::visit(overloaded {
			[&](std::variant<PenStroke*,EraserStroke*> st) {
				PathStroke* pst = convert_variant<PathStroke*>(st);
				Point old = pst->points().back();
				pst->push_back(p);
				assert(m_page_picture->layers().size() == 1);
				m_page_picture->layers()[0]->draw_line(old, p, st);
			}
		}, get(m_current_stroke.value()));
	}
}

void PageWidget::finish_path()
{
	if (!m_current_stroke)
		return;
	// TODO This unnecessarily redraws the stroke!
	std::visit(overloaded {
		[&](std::unique_ptr<PenStroke> &st) {
			m_view->undoStack->push(new AddPenStrokeCommand(m_view->doc.get(), page_index, 0, std::move(st)));
		},
		[&](std::unique_ptr<EraserStroke> &st) {
			m_view->undoStack->push(new AddEraserStrokeCommand(m_view->doc.get(), page_index, 0, std::move(st)));
		}
	}, m_current_stroke.value());
	m_current_stroke.reset();
}
