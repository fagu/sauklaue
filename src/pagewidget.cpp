#include "pagewidget.h"

#include "util.h"
#include "mainwindow.h"
#include "actions.h"

#include <QtWidgets>


LayerPicture::LayerPicture(NormalLayer* layer, PagePicture* page_picture) :
	m_layer(layer),
	m_page_picture(page_picture)
{
	connect(layer, &NormalLayer::stroke_added, this, &LayerPicture::stroke_added);
	connect(layer, &NormalLayer::stroke_deleting, this, &LayerPicture::stroke_deleting);
	
	cairo_surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, m_page_picture->width, m_page_picture->height);
	cr = Cairo::Context::create(cairo_surface);
// 	cr->set_antialias(Cairo::ANTIALIAS_GRAY);
	cr->set_line_cap(Cairo::LINE_CAP_ROUND);
	cr->set_line_join(Cairo::LINE_JOIN_ROUND);
	cr->save();
	setup();
	for (int i = 0; i < (int)m_layer->strokes().size(); i++)
		draw_stroke(i);
}

void LayerPicture::stroke_added()
{
	draw_stroke((int)m_layer->strokes().size()-1);
}

void LayerPicture::stroke_deleting()
{
	// TODO This redraws the entire page!
	// Since we know the stroke that is about to be deleted, only part of the page would need to be updated.
	setup();
	for (int i = 0; i < (int)m_layer->strokes().size()-1; i++) // Don't draw the last stroke!
		draw_stroke(i);
	emit update(QRect(0,0,m_page_picture->width,m_page_picture->height));
}

const int DEFAULT_LINE_WIDTH = 1500;
const int DEFAULT_ERASER_WIDTH = 1500*20;

void LayerPicture::setup()
{
	cr->restore();
	cr->save();
	cr->transform(m_page_picture->page2widget);
	{
		CairoGroup cg(cr);
		cr->set_source_rgba(0,0,0,0);
		cr->set_operator(Cairo::OPERATOR_SOURCE);
		cr->paint(); // Reset every pixel to transparent.
	}
	
	cr->rectangle(0,0,m_page_picture->page->width(),m_page_picture->page->height());
	cr->clip();
}

void LayerPicture::draw_path(const std::vector<Point> &points) {
	assert(!points.empty());
	cr->move_to(points[0].x, points[0].y);
	if (points.size() == 1) {
		cr->line_to(points[0].x, points[0].y);
	} else {
		for (size_t i = 1; i < points.size(); i++)
			cr->line_to(points[i].x, points[i].y);
	}
	// It might be better to compute the union of the extents of the individual segments.
	double x1, y1, x2, y2;
	cr->get_stroke_extents(x1, y1, x2, y2);
	cr->stroke();
	updatePageRect(x1, y1, x2, y2);
}

void LayerPicture::draw_stroke(int i)
{
	CairoGroup cg(cr);
	ptr_Stroke stroke = get(m_layer->strokes()[i]);
	std::visit(overloaded {
		[&](const PenStroke* st) {
			cr->set_line_width(st->width());
			Color co = st->color();
			cr->set_source_rgba(co.r(), co.g(), co.b(), co.a());
			draw_path(st->points());
		},
		[&](const EraserStroke* st) {
			cr->set_line_width(st->width());
			cr->set_source_rgba(0,0,0,0); // Transparent
			// Replace layer color by transparent instead of making a transparent drawing on top of the layer.
			cr->set_operator(Cairo::OPERATOR_SOURCE);
			draw_path(st->points());
		}
	}, stroke);
}

void LayerPicture::draw_line(Point a, Point b, ptr_Stroke stroke)
{
	CairoGroup cg(cr);
	std::visit(overloaded {
		[&](const PenStroke* st) {
			cr->set_line_width(st->width());
			Color co = st->color();
			cr->set_source_rgba(co.r(), co.g(), co.b(), co.a());
		},
		[&](const EraserStroke* st) {
			cr->set_line_width(st->width());
			cr->set_source_rgba(0,0,0,0); // Transparent
			// Replace layer color by transparent instead of making a transparent drawing on top of the layer.
			cr->set_operator(Cairo::OPERATOR_SOURCE);
		}
	}, stroke);
	cr->move_to(a.x, a.y);
	cr->line_to(b.x, b.y);
	double x1, y1, x2, y2;
	cr->get_stroke_extents(x1, y1, x2, y2);
	cr->stroke();
	updatePageRect(x1, y1, x2, y2);
}

void LayerPicture::bounding_rect_helper(double x, double y, double &minx, double &maxx, double &miny, double &maxy) {
	m_page_picture->page2widget.transform_point(x, y);
	if (minx > x)
		minx = x;
	if (maxx < x)
		maxx = x;
	if (miny > y)
		miny = y;
	if (maxy < y)
		maxy = y;
}

void LayerPicture::updatePageRect(double x1, double y1, double x2, double y2)
{
	double minx = +1./0, maxx = -1./0, miny = +1./0, maxy = -1./0;
	bounding_rect_helper(x1, y1, minx, maxx, miny, maxy);
	bounding_rect_helper(x1, y2, minx, maxx, miny, maxy);
	bounding_rect_helper(x2, y1, minx, maxx, miny, maxy);
	bounding_rect_helper(x2, y2, minx, maxx, miny, maxy);
	emit update(QRect(minx-1, miny-1, maxx-minx+3, maxy-miny+3));
}

PagePicture::PagePicture(Page* _page, int _width, int _height) :
	page(_page),
	width(_width),
	height(_height)
{
	assert(page);
	assert(width >= 20 && height >= 20);
	qDebug() << "size" << width << height;
	int margin = 5;
	int rem_width = width - 2*margin;
	int rem_height = height - 2*margin;
	double scale = std::min((double)rem_width/page->width(), (double)rem_height/page->height());
	double disp_width = scale*page->width();
	double disp_height = scale*page->height();
	double dx = (width-disp_width)/2;
	double dy = (height-disp_height)/2;
	page2widget = Cairo::translation_matrix(dx, dy);
	page2widget.scale(scale, scale);
	widget2page = page2widget;
	widget2page.invert();
	
	for (size_t i = 0; i < page->layers().size(); i++)
		register_layer(i);
	
	connect(page, &Page::layer_added, this, &PagePicture::register_layer);
	connect(page, &Page::layer_deleted, this, &PagePicture::unregister_layer);
}

void PagePicture::register_layer(int index)
{
	layers.emplace(layers.begin() + index, std::make_unique<LayerPicture>(page->layer(index), this));
	connect(layers[index].get(), &LayerPicture::update, this, &PagePicture::update_layer);
}

void PagePicture::unregister_layer(int index)
{
	layers.erase(layers.begin() + index);
}

void PagePicture::update_layer(const QRect& rect)
{
	emit update(rect);
}



PageWidget::PageWidget(MainWindow* view) :
	QWidget(nullptr),
	m_view(view)
{
	setMinimumSize(20,20);
}

void PageWidget::setPage(Page* page)
{
	m_page = page;
	m_current_path.reset();
	if (page) {
		m_page_picture = std::make_unique<PagePicture>(page, width(), height());
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


// Tablet size: 28cm x 15.8cm
void PageWidget::update_tablet_map()
{
	if (!m_page_picture) {
		QProcess::execute("xinput", {"set-prop", "XPPEN Tablet Pen (0)", "--type=float", "Coordinate Transformation Matrix", "1", "0", "0", "0", "1", "0", "0", "0", "1"});
		return;
	}
	double x = 0, y = 0;
	m_page_picture->page2widget.transform_point(x, y);
	QPoint topleft = mapToGlobal(QPoint(x,y));
	x = m_page->width(); y = m_page->height();
	m_page_picture->page2widget.transform_point(x, y);
	QPoint bottomright = mapToGlobal(QPoint(x,y));
// 	qDebug() << "map" <<  << ;
	double tablet_margin_left = 0.1, tablet_margin_right = 0.1, tablet_margin_top = 0.1, tablet_margin_bottom = 0.1;
	QRect scr = screen()->geometry();
	double x1 = (double)topleft.x() / scr.width();
	double x2 = (double)bottomright.x() / scr.width();
	double y1 = (double)topleft.y() / scr.height();
	double y2 = (double)bottomright.y() / scr.height();
	double a = (x1-x2)/(1-tablet_margin_left-tablet_margin_right);
	double b = (x2*(1-tablet_margin_left) - x1*tablet_margin_right)/(1-tablet_margin_left-tablet_margin_right);
	double c = (y2-y1)/(1-tablet_margin_top-tablet_margin_bottom);
	double d = (y1*(1-tablet_margin_bottom) - y2*tablet_margin_top)/(1-tablet_margin_top-tablet_margin_bottom);
	QProcess::execute("xinput", {"set-prop", "XPPEN Tablet Pen (0)", "--type=float", "Coordinate Transformation Matrix", "0", QString::number(a), QString::number(b), QString::number(c), "0", QString::number(d), "0", "0", "1"});
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
	for (const auto& layer_picture : m_page_picture->layers) {
		layer_picture->cairo_surface->flush();
		if (width() != layer_picture->cairo_surface->get_width() || height() != layer_picture->cairo_surface->get_height()) {
			qDebug() << "size has recently changed";
			return;
		}
		qDebug() << "Draw layer";
		QImage img((const uchar*)layer_picture->cairo_surface->get_data(), width(), height(), QImage::Format_ARGB32_Premultiplied);
		painter.drawImage(0, 0, img);
	}
}

void PageWidget::resizeEvent(QResizeEvent* event)
{
	qDebug() << "resize" << event->size();
	setPage(m_page);
}

void PageWidget::moveEvent(QMoveEvent* event)
{
	update_tablet_map();
}

void PageWidget::mousePressEvent(QMouseEvent* event)
{
	qDebug() << "Mouse event";
	if (event->button() == Qt::LeftButton)
		start_path(event->pos().x(), event->pos().y(), StrokeType::Pen);
	else if (event->button() == Qt::RightButton)
		start_path(event->pos().x(), event->pos().y(), StrokeType::Eraser);
}

void PageWidget::mouseMoveEvent(QMouseEvent* event)
{
	qDebug() << "Mouse event";
	continue_path(event->pos().x(), event->pos().y());
}

void PageWidget::mouseReleaseEvent(QMouseEvent* event)
{
	qDebug() << "Mouse event";
	finish_path();
}

void PageWidget::tabletEvent(QTabletEvent* event)
{
	if (event->pointerType() == QTabletEvent::Pen || event->pointerType() == QTabletEvent::Eraser) {
		qDebug() << "Tablet pen event" << event->pressure();
		event->pressure();
		if (event->type() == QEvent::TabletPress) {
			// If we push the right button while touching the surface, the left button is automatically pushed as well.
			if (event->button() == Qt::LeftButton) { // Do not draw if we only press the right button while hovering.
				qDebug() << "press";
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
			qDebug() << "move";
			continue_path(event->posF().x(), event->posF().y());
			event->accept();
		} else if (event->type() == QEvent::TabletRelease) {
			qDebug() << "release";
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
	if (!m_current_path) {
		qDebug() << "draw at" << x << y;
		Point p(x,y);
		if (type == StrokeType::Pen) {
			m_current_path = std::make_unique<PenStroke>(DEFAULT_LINE_WIDTH, Color::BLACK);
		} else if (type == StrokeType::Eraser) {
			m_current_path = std::make_unique<EraserStroke>(DEFAULT_ERASER_WIDTH);
		}
		std::visit(overloaded {
			[&](std::variant<PenStroke*,EraserStroke*> st) {
				PathStroke* pst = convert_variant<PathStroke*>(st);
				pst->push_back(p);
				assert(m_page_picture->layers.size() == 1);
				m_page_picture->layers[0]->draw_line(p, p, st);
			}
		}, get(m_current_path.value()));
	}
}

void PageWidget::continue_path(double x, double y)
{
	if (!m_current_path)
		return;
	m_page_picture->widget2page.transform_point(x, y);
	if (m_current_path) {
		qDebug() << "draw stroke";
		Point p(x,y);
		std::visit(overloaded {
			[&](std::variant<PenStroke*,EraserStroke*> st) {
				PathStroke* pst = convert_variant<PathStroke*>(st);
				Point old = pst->points().back();
				pst->push_back(p);
				assert(m_page_picture->layers.size() == 1);
				m_page_picture->layers[0]->draw_line(old, p, st);
			}
		}, get(m_current_path.value()));
	}
}

void PageWidget::finish_path()
{
	if (!m_current_path)
		return;
	// TODO This unnecessarily redraws the stroke!
	std::visit(overloaded {
		[&](std::unique_ptr<PenStroke> &st) {
			m_view->undoStack->push(new AddPenStrokeCommand(m_view->doc.get(), m_view->current_page, 0, std::move(st)));
		},
		[&](std::unique_ptr<EraserStroke> &st) {
			m_view->undoStack->push(new AddEraserStrokeCommand(m_view->doc.get(), m_view->current_page, 0, std::move(st)));
		}
	}, m_current_path.value());
	m_current_path.reset();
}
