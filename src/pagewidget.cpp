#include "pagewidget.h"

#include "mainwindow.h"
#include "commands.h"
#include "tablet.h"
#include "document.h"
#include "renderer.h"
#include "tool-state.h"

#include <QScreen>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>

// const int DEFAULT_LINE_WIDTH = 1500;
const int DEFAULT_ERASER_WIDTH = 1500 * 20;

StrokeCreator::StrokeCreator(unique_ptr_Stroke stroke, std::function<void(unique_ptr_Stroke)> committer, DrawingLayerPicture* pic) :
    m_stroke(std::move(stroke)), m_committer(committer), m_pic(pic) {
	assert(m_pic);
	m_pic->set_current_stroke(get(m_stroke));
}

StrokeCreator::~StrokeCreator() {
	if (m_pic)
		m_pic->reset_current_stroke();
}

void StrokeCreator::commit() {
	m_committer(std::move(m_stroke));
	m_pic = nullptr;
}

void StrokeCreator::add_point(Point p) {
	PathStroke* pst = convert_variant<PathStroke*>(get(m_stroke));
	Point old = pst->points().empty() ? p : pst->points().back();
	pst->push_back(p);
	// TODO This doesn't work properly with transparency because the old stroke is partially covered by the new line, making some parts more opaque than they should be.
	// Really, when using transparency, I guess we should clip to the region of the new line segment and then redraw the entire layer on that clipping region.
	// Even when the line is completely opaque, this is probably technically wrong because of antialiasing (which makes the line somewhat transparent near the boundary).
	m_pic->draw_line(old, p, get(m_stroke));
}

double EraserCursor::radius() const {
	return m_width * transformation()->unit2pixel / 2;
}

QRect EraserCursor::rect() const {
	double r = radius();
	return QRect((pos() - QPointF(r+1,r+1)).toPoint(), (pos() + QPointF(r+1,r+1)).toPoint());
}

void EraserCursor::paint(QPainter& painter) const {
	painter.save();
	painter.setClipRegion(transformation()->image_rect);
	painter.setPen(QPen(Qt::black, 1));
	painter.setBrush(Qt::white);
	painter.setOpacity(0.5);
	double r = radius();
	painter.drawEllipse(pos(), r, r);
	painter.restore();
}

PageWidget::PageWidget(ToolState* toolState) :
    QWidget(nullptr),
    m_tool_state(toolState) {
	setMinimumSize(20, 20);
	connect(m_tool_state, &ToolState::blackboardModeToggled, this, qOverload<>(&QWidget::update));
}

void PageWidget::setPage(SPage* page) {
	if (m_page == page)
		return;  // Do nothing. In particular, don't clear m_current_stroke.
	m_page = page;
	if (!m_page)
		assert(!has_focus);
	m_current_stroke.reset();
	setupPicture();
}

void PageWidget::setupPicture() {
	if (m_page) {
		set_tool_cursor(nullptr);
		m_page_picture = std::make_unique<PagePicture>(m_page, width(), height());
		connect(m_page_picture.get(), &PagePicture::update, this, &PageWidget::update_page);
	} else {
		m_page_picture = nullptr;
	}
	update_tablet_map();
	update();
}

void PageWidget::update_page(const QRect& rect) {
	update(rect.translated(m_page_picture->transformation().topLeft));
}

void PageWidget::removing_layer_picture(ptr_LayerPicture layer_picture) {
	std::visit(overloaded{[&](DrawingLayerPicture* layer_picture) {
		                      if (layer_picture == m_current_stroke->pic())  // Removing the layer we're currently drawing on. => Stop drawing.
			                      m_current_stroke.reset();
	                      },
	                      [&](PDFLayerPicture*) {
	                      }},
	           layer_picture);
}

QRectF PageWidget::minimum_rect_in_pixels() {
	static const int MARGIN = 50;
	QPoint delta(MARGIN, MARGIN);
	return QRectF(mapToGlobal(QPoint(0,0)) - delta, mapToGlobal(QPoint(width(), height())) + delta);
}

void PageWidget::update_tablet_map() {
	if (!has_focus)
		return;
	assert(m_page);
	TabletHandler::self()->set_active_region(minimum_rect_in_pixels(), screen()->virtualSize());
}

void PageWidget::focusPage() {
	has_focus = true;
	update_tablet_map();
	update();
}

void PageWidget::unfocusPage() {
	has_focus = false;
	update_tablet_map();
	update();
}

void PageWidget::paintEvent([[maybe_unused]] QPaintEvent* event) {
	// 	qDebug() << "paint" << event->region();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, false);
	// Background around the pages
	painter.fillRect(0, 0, width(), height(), QColorConstants::LightGray);
	if (!m_page)
		return;
	// Page background color
	painter.fillRect(m_page_picture->transformation().image_rect, m_tool_state->blackboardMode() ? QColorConstants::Black : QColorConstants::White);
	// Draw ordinary layers.
	for (auto layer_picture : m_page_picture->layers()) {
		LayerPicture* base_layer_picture = convert_variant<LayerPicture*>(layer_picture);
		painter.drawImage(m_page_picture->transformation().topLeft, base_layer_picture->img());
	}
	// Draw the temporary layer (semi-transparent).
	{
		painter.setOpacity(0.3);
		auto layer_picture = m_page_picture->temporary_layer();
		painter.drawImage(m_page_picture->transformation().topLeft, layer_picture->img());
		painter.setOpacity(1);
	}
	// Draw the tool cursor.
	if (m_tool_cursor)
		m_tool_cursor->paint(painter);
	// Draw red border around focused pages.
	if (has_focus) {
		painter.setPen(QPen(QColorConstants::Red, 3));
		painter.drawRect(m_page_picture->transformation().image_rect);
	}
}

void PageWidget::resizeEvent(QResizeEvent*) {
	// 	qDebug() << "resize" << event->size();
	setupPicture();
}

void PageWidget::moveEvent(QMoveEvent*) {
	update_tablet_map();
}

void PageWidget::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton)
		start_path(event->pos(), StrokeType::Pen);
	else if (event->button() == Qt::RightButton) {
		start_path(event->pos(), StrokeType::Eraser);
		if (m_page)
			set_tool_cursor(std::make_unique<EraserCursor>(event->pos(), &m_page_picture->transformation(), DEFAULT_ERASER_WIDTH));
	} else if (event->button() == Qt::MiddleButton)
		start_path(event->pos(), StrokeType::LaserPointer);
	if (m_page)
		emit focus();
}

void PageWidget::mouseMoveEvent(QMouseEvent* event) {
	move_tool_cursor(event->pos());
	continue_path(event->pos());
}

void PageWidget::mouseReleaseEvent(QMouseEvent*) {
	set_tool_cursor(nullptr);
	finish_path();
}

void PageWidget::tabletEvent(QTabletEvent* event) {
	if (event->pointerType() == QTabletEvent::Pen || event->pointerType() == QTabletEvent::Eraser) {
		if (event->type() == QEvent::TabletPress) {
			// If we push the right button while touching the surface, the left button is automatically pushed as well.
			if (event->button() == Qt::LeftButton) {  // Do not draw if we only press the right button while hovering.
				StrokeType type = StrokeType::Pen;
				if (event->pointerType() == QTabletEvent::Eraser || (event->buttons() & Qt::RightButton))
					type = StrokeType::Eraser;
				start_path(event->posF(), type);
				event->accept();
			} else if (event->button() == Qt::RightButton) {
				if (m_page)
					set_tool_cursor(std::make_unique<EraserCursor>(event->posF(), &m_page_picture->transformation(), DEFAULT_ERASER_WIDTH));
				event->accept();
			} else if (event->button() == Qt::MiddleButton) {
				start_path(event->posF(), StrokeType::LaserPointer);
				event->accept();
			}
		} else if (event->type() == QEvent::TabletMove) {
			move_tool_cursor(event->posF());
			continue_path(event->posF());
			event->accept();
		} else if (event->type() == QEvent::TabletRelease) {
			finish_path();
			if (!(event->buttons() & Qt::RightButton))
				set_tool_cursor(nullptr);
			event->accept();
		}
	}
}

void PageWidget::start_path(QPointF pp, StrokeType type) {
	if (!m_page)
		return;
	if (!m_current_stroke) {
		Point p = m_page_picture->transformation().widget2page(pp);
		unique_ptr_Stroke stroke;
		int timeout;
		if (type == StrokeType::Pen) {
			stroke = std::make_unique<PenStroke>(m_tool_state->penSize(), m_tool_state->penColor());
			timeout = -1;
		} else if (type == StrokeType::Eraser) {
			stroke = std::make_unique<EraserStroke>(DEFAULT_ERASER_WIDTH);
			timeout = -1;
		} else if (type == StrokeType::LaserPointer) {
			QColor color = QColorConstants::Red;
			stroke = std::make_unique<PenStroke>(m_tool_state->penSize() * 4, color);
			timeout = 3000;  // 3 second timeout
		} else {
			assert(false);
		}
		if (timeout == -1) {
			int layer_index = -1;
			for (size_t i = 0; i < m_page->layers().size(); i++) {
				if (std::holds_alternative<NormalLayer*>(m_page->layers()[i])) {
					layer_index = i;
					break;
				}
			}
			if (layer_index == -1)
				return;
			auto layer_picture = std::get<DrawingLayerPicture*>(m_page_picture->layers()[layer_index]);
			auto layer = std::get<NormalLayer*>(m_page->layers()[layer_index]);
			m_current_stroke.emplace(
			        std::move(stroke), [this, layer](unique_ptr_Stroke st) {
				        m_tool_state->undoStack()->push(new AddStrokeCommand(layer, std::move(st)));
			        },
			        layer_picture);
		} else {
			auto layer_picture = m_page_picture->temporary_layer();
			auto layer = m_page->temporary_layer();
			m_current_stroke.emplace(
			        std::move(stroke), [layer, timeout](unique_ptr_Stroke st) {
				        layer->add_stroke(std::move(st), timeout);
			        },
			        layer_picture);
		}
		m_current_stroke->add_point(p);
	}
}

void PageWidget::continue_path(QPointF pp) {
	if (!m_current_stroke)
		return;
	Point p = m_page_picture->transformation().widget2page(pp);
	m_current_stroke->add_point(p);
}

void PageWidget::finish_path() {
	if (!m_current_stroke)
		return;
	m_current_stroke->commit();
	m_current_stroke.reset();
}

void PageWidget::set_tool_cursor(std::unique_ptr<ToolCursor> tool_cursor) {
	std::optional<QRect> old;
	if (m_tool_cursor)
		old = m_tool_cursor->rect();
	m_tool_cursor = std::move(tool_cursor);
	if (old)
		update(old.value());
	if (m_tool_cursor) {
		connect(m_tool_cursor.get(), &ToolCursor::update, this, qOverload<const QRect&>(&PageWidget::update));
		update(m_tool_cursor->rect());
	}
}

void PageWidget::move_tool_cursor(QPointF pos) {
	if (m_tool_cursor)
		m_tool_cursor->move(pos);
}
