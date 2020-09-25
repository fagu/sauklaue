#include "pagewidget.h"

#include "util.h"
#include "mainwindow.h"
#include "commands.h"
#include "tablet.h"

#include <QScreen>
#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>


// const int DEFAULT_LINE_WIDTH = 1500;
const int DEFAULT_ERASER_WIDTH = 1500*20;


StrokeCreator::StrokeCreator(unique_ptr_Stroke stroke, std::function<void(unique_ptr_Stroke)> committer, DrawingLayerPicture *pic) : m_stroke(std::move(stroke)), m_committer(committer), m_pic(pic) {
	assert(m_pic);
	m_pic->set_current_stroke(get(m_stroke));
}

StrokeCreator::~StrokeCreator() {
	if (m_pic)
		m_pic->reset_current_stroke();
}

void StrokeCreator::commit()
{
	m_committer(std::move(m_stroke));
	m_pic = nullptr;
}

void StrokeCreator::add_point(Point p)
{
	PathStroke* pst = convert_variant<PathStroke*>(get(m_stroke));
	Point old = pst->points().empty() ? p : pst->points().back();
	pst->push_back(p);
	// TODO This doesn't work properly with transparency because the old stroke is partially covered by the new line, making some parts more opaque than they should be.
	// Really, when using transparency, I guess we should clip to the region of the new line segment and then redraw the entire layer on that clipping region.
	// Even when the line is completely opaque, this is probably technically wrong because of antialiasing (which makes the line somewhat transparent near the boundary).
	m_pic->draw_line(old, p, get(m_stroke));
}


PageWidget::PageWidget(MainWindow* view, int view_index) :
	QWidget(nullptr),
	m_view(view),
	m_view_index(view_index)
{
	setMinimumSize(20,20);
	connect(view, &MainWindow::blackboardModeToggled, this, qOverload<>(&QWidget::update));
}

void PageWidget::setPage(SPage* page, int index)
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
	update(rect.translated(m_page_picture->transformation().topLeft));
}

void PageWidget::removing_layer_picture(ptr_LayerPicture layer_picture)
{
	std::visit(overloaded {
		[&](DrawingLayerPicture* layer_picture) {
			if (layer_picture == m_current_stroke->pic()) // Removing the layer we're currently drawing on. => Stop drawing.
				m_current_stroke.reset();
		},
		[&](PDFLayerPicture*) {
		}
	}, layer_picture);
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
	return m_page_picture->transformation().page2widget * Cairo::translation_matrix(translation.x(), translation.y());
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
	QSize scr = screen()->virtualSize();
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
	// Background around the pages
	painter.fillRect(0,0,width(),height(), QColorConstants::LightGray);
	if (!m_page_picture) {
		return;
	}
// 	int x1 = m_page_picture->dx, y1 = m_page_picture->dy, x2 = m_page_picture->dx+m_page_picture->page_width, y2 = m_page_picture->dx+m_page_picture->page_height;
	// Page background color
	painter.fillRect(m_page_picture->transformation().image_rect, m_view->blackboardMode() ? QColorConstants::Black : QColorConstants::White);
	for (auto layer_picture : m_page_picture->layers()) {
		LayerPicture* base_layer_picture = convert_variant<LayerPicture*>(layer_picture);
		painter.drawImage(m_page_picture->transformation().topLeft, base_layer_picture->img());
	}
	// We draw the temporary layer semi-transparent.
	painter.setOpacity(0.3);
	{
		auto layer_picture = m_page_picture->temporary_layer();
		painter.drawImage(m_page_picture->transformation().topLeft, layer_picture->img());
	}
	painter.setOpacity(1);
	if (has_focus) {
		painter.setPen(QPen(QColorConstants::Red, 3));
		painter.drawRect(m_page_picture->transformation().image_rect);
	}
}

void PageWidget::resizeEvent(QResizeEvent* )
{
// 	qDebug() << "resize" << event->size();
	setupPicture();
}

void PageWidget::moveEvent(QMoveEvent* )
{
	update_tablet_map();
}

void PageWidget::mousePressEvent(QMouseEvent* event)
{
	if (has_focus && event->button() == Qt::LeftButton)
		start_path(event->pos(), StrokeType::Pen);
	else if (has_focus && event->button() == Qt::RightButton)
		start_path(event->pos(), StrokeType::Eraser);
	else if (event->button() == Qt::MiddleButton)
		start_path(event->pos(), StrokeType::LaserPointer);
}

void PageWidget::mouseMoveEvent(QMouseEvent* event)
{
	continue_path(event->pos());
}

void PageWidget::mouseReleaseEvent(QMouseEvent* )
{
	finish_path();
}

void PageWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (page_index != -1 && event->button() == Qt::LeftButton)
		m_view->focusView(m_view_index);
}

void PageWidget::tabletEvent(QTabletEvent* event)
{
	if (event->pointerType() == QTabletEvent::Pen || event->pointerType() == QTabletEvent::Eraser) {
		if (event->type() == QEvent::TabletPress) {
			// If we push the right button while touching the surface, the left button is automatically pushed as well.
			if (has_focus && event->button() == Qt::LeftButton) { // Do not draw if we only press the right button while hovering.
				StrokeType type = StrokeType::Pen;
				if (event->pointerType() == QTabletEvent::Eraser || (event->buttons() & Qt::RightButton))
					type = StrokeType::Eraser;
				start_path(event->posF(), type);
				event->accept();
			} else if (event->button() == Qt::RightButton) {
				if (has_focus)
					setCursor(Qt::CrossCursor);
				event->accept();
			} else if (event->button() == Qt::MiddleButton) {
				start_path(event->posF(), StrokeType::LaserPointer);
				event->accept();
			}
		} else if (event->type() == QEvent::TabletMove) {
			continue_path(event->posF());
			event->accept();
		} else if (event->type() == QEvent::TabletRelease) {
			finish_path();
			if (event->button() == Qt::RightButton)
				unsetCursor();
			event->accept();
		}
	}
}

int first_normal_layer_index(SPage* page) {
	for (size_t i = 0; i < page->layers().size(); i++) {
		if (std::holds_alternative<NormalLayer*>(page->layers()[i]))
			return i;
	}
	assert(false); // No NormalLayer present => Don't know what to draw on.
}

void PageWidget::start_path(QPointF pp, StrokeType type)
{
	if (!m_page_picture)
		return;
	if (!m_current_stroke) {
		Point p = m_page_picture->transformation().widget2page(pp);
		unique_ptr_Stroke stroke;
		int timeout;
		if (type == StrokeType::Pen) {
			stroke = std::make_unique<PenStroke>(m_view->penSize(), m_view->penColor());
			timeout = -1;
		} else if (type == StrokeType::Eraser) {
			stroke = std::make_unique<EraserStroke>(DEFAULT_ERASER_WIDTH);
			timeout = -1;
		} else if (type == StrokeType::LaserPointer) {
			QColor color = QColorConstants::Red;
// 			color.setAlphaF(0.3);
			stroke = std::make_unique<PenStroke>(m_view->penSize() * 4, color);
			timeout = 3000; // 3 second timeout
		} else {
			assert(false);
		}
		if (timeout == -1) {
			auto layer_picture = std::get<DrawingLayerPicture*>(m_page_picture->layers()[first_normal_layer_index(m_page)]);
			auto layer = std::get<NormalLayer*>(m_page->layers()[first_normal_layer_index(m_page)]);
			m_current_stroke.emplace(std::move(stroke), [this,layer](unique_ptr_Stroke st) {
				m_view->undoStack->push(new AddStrokeCommand(layer, std::move(st)));
			}, layer_picture);
		} else {
			auto layer_picture = m_page_picture->temporary_layer();
			auto layer = m_page->temporary_layer();
			m_current_stroke.emplace(std::move(stroke), [layer,timeout](unique_ptr_Stroke st) {
				layer->add_stroke(std::move(st), timeout);
			}, layer_picture);
		}
		m_current_stroke->add_point(p);
	}
}

void PageWidget::continue_path(QPointF pp)
{
	if (!m_current_stroke)
		return;
	Point p = m_page_picture->transformation().widget2page(pp);
	m_current_stroke->add_point(p);
}

void PageWidget::finish_path()
{
	if (!m_current_stroke)
		return;
	m_current_stroke->commit();
	m_current_stroke.reset();
}
