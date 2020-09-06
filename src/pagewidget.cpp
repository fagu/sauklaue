#include "pagewidget.h"

#include "sauklaue.h"
#include "actions.h"

#include <QtWidgets>


PageWidget::PageWidget(sauklaue* view) :
	QWidget(nullptr),
	m_view(view)
{
	setMinimumSize(20,20);
}

void PageWidget::setPage(Page* page)
{
	if (m_page) {
		for (size_t i = 0; i < page->layers().size(); i++)
			unregister_layer(i);
		disconnect(page, 0, 0, 0);
	}
	m_page = page;
	if (page) {
		connect(page, &Page::layer_added, this, &PageWidget::register_layer);
		connect(page, &Page::layer_deleted, this, &PageWidget::unregister_layer);
		for (size_t i = 0; i < page->layers().size(); i++)
			register_layer(i);
	}
	setupCairo();
	update_tablet_map();
}

void PageWidget::register_layer(int index)
{
	connect(m_page->layer(index), &NormalLayer::stroke_added, this, &PageWidget::stroke_added);
	connect(m_page->layer(index), &NormalLayer::stroke_deleted, this, &PageWidget::stroke_deleted);
}

void PageWidget::unregister_layer(int index)
{
	disconnect(m_page->layer(index), 0, 0, 0);
}

void PageWidget::stroke_added()
{
	Stroke *stroke = ;
	const auto & points = path->points();
	assert(!path->points().empty());
	if (points.size() == 1) {
		draw_line(points[0], points[0]);
	} else {
		for (size_t i = 0; i+1 < points.size(); i++)
			draw_line(points[i], points[i+1]);
	}
}

void PageWidget::stroke_deleted()
{
}


// Tablet size: 28cm x 15.8cm
void PageWidget::update_tablet_map()
{
	if (!m_page) {
		QProcess::execute("xinput", {"set-prop", "XPPEN Tablet Pen (0)", "--type=float", "Coordinate Transformation Matrix", "1", "0", "0", "0", "1", "0", "0", "0", "1"});
		return;
	}
	double x = 0, y = 0;
	page2widget.transform_point(x, y);
	QPoint topleft = mapToGlobal(QPoint(x,y));
	x = m_page->width(); y = m_page->height();
	page2widget.transform_point(x, y);
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
	cairo_surface->flush();
	if (width() != cairo_surface->get_width() || height() != cairo_surface->get_height()) {
		qDebug() << "size has recently changed";
		return;
	}
	QImage img((const uchar*)cairo_surface->get_data(), width(), height(), QImage::Format_ARGB32_Premultiplied);
	QPainter painter(this);
	painter.drawImage(0, 0, img);
}

void PageWidget::resizeEvent(QResizeEvent* event)
{
	qDebug() << "resize" << event->size();
	setupCairo();
	update_tablet_map();
}

void PageWidget::moveEvent(QMoveEvent* event)
{
	update_tablet_map();
}

void PageWidget::mousePressEvent(QMouseEvent* event)
{
	if (!m_page)
		return;
	double x = event->pos().x(), y = event->pos().y();
	widget2page.transform_point(x, y);
	if (!pressed) {
		qDebug() << "draw at" << x << y;
		Point p(x,y);
		Point old = add_to_path(p);
		draw_line(old, p);
	}
	pressed = true;
}

void PageWidget::mouseReleaseEvent(QMouseEvent* event)
{
	if (!m_page)
		return;
	if (pressed)
		finish_path();
	pressed = false;
}

void PageWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (!m_page)
		return;
	double x = event->pos().x(), y = event->pos().y();
	widget2page.transform_point(x, y);
	if (pressed) {
		qDebug() << "draw stroke";
		Point p(x,y);
		Point old = add_to_path(p);
		draw_line(old, p);
	}
}

void PageWidget::setupCairo()
{
	int width = this->width();
	int height = this->height();
	assert(width >= 20 && height >= 20);
	int margin = 5;
	int rem_width = width - 2*margin;
	int rem_height = height - 2*margin;
	cairo_surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, width, height);
	cr = Cairo::Context::create(cairo_surface);
	cr->save();
	cr->set_source_rgb(0.8,0.8,0.8);
	cr->paint();
	cr->restore();
	if (m_page) {
		double scale = std::min((double)rem_width/m_page->width(), (double)rem_height/m_page->height());
		double disp_width = scale*m_page->width();
		double disp_height = scale*m_page->height();
		double dx = (width-disp_width)/2;
		double dy = (height-disp_height)/2;
		page2widget = Cairo::translation_matrix(dx, dy);
		page2widget.scale(scale, scale);
		widget2page = page2widget;
		widget2page.invert();
		cr->transform(page2widget);
		cr->rectangle(0,0,m_page->width(),m_page->height());
		cr->clip_preserve();
		cr->save();
		cr->set_source_rgb(1,1,1);
		cr->fill();
		cr->restore();
	// 	cr->set_antialias(Cairo::ANTIALIAS_GRAY);
		cr->set_line_cap(Cairo::LINE_CAP_ROUND);
		cr->set_line_join(Cairo::LINE_JOIN_ROUND);
		cr->set_line_width(10);
		for (const std::unique_ptr<Stroke>& path : m_page->layer(0)->strokes()) {
			const auto & points = path->points();
			assert(!path->points().empty());
			if (points.size() == 1) {
				draw_line(points[0], points[0]);
			} else {
				for (size_t i = 0; i+1 < points.size(); i++)
					draw_line(points[i], points[i+1]);
			}
		}
		if (m_current_path) {
			const auto & points = m_current_path->points();
			assert(!m_current_path->points().empty());
			if (points.size() == 1) {
				draw_line(points[0], points[0]);
			} else {
				for (size_t i = 0; i+1 < points.size(); i++)
					draw_line(points[i], points[i+1]);
			}
		}
	}
	update();
}

void PageWidget::draw_line(Point a, Point b)
{
	cr->move_to(a.x, a.y);
	cr->line_to(b.x, b.y);
	double x1, y1, x2, y2;
	cr->get_stroke_extents(x1, y1, x2, y2);
	cr->stroke();
	updatePageRect(x1, y1, x2, y2);
}

void PageWidget::bounding_rect_helper(double x, double y, double &minx, double &maxx, double &miny, double &maxy) {
	page2widget.transform_point(x, y);
	if (minx > x)
		minx = x;
	if (maxx < x)
		maxx = x;
	if (miny > y)
		miny = y;
	if (maxy < y)
		maxy = y;
}

void PageWidget::updatePageRect(double x1, double y1, double x2, double y2)
{
	double minx = +1./0, maxx = -1./0, miny = +1./0, maxy = -1./0;
	bounding_rect_helper(x1, y1, minx, maxx, miny, maxy);
	bounding_rect_helper(x1, y2, minx, maxx, miny, maxy);
	bounding_rect_helper(x2, y1, minx, maxx, miny, maxy);
	bounding_rect_helper(x2, y2, minx, maxx, miny, maxy);
	update(minx-1, miny-1, maxx-minx+3, maxy-miny+3);
}

Point PageWidget::add_to_path(Point p)
{
	Point old = p;
	if (m_current_path)
		old = m_current_path->points().back();
	else
		m_current_path = std::make_unique<Stroke>(10);
	m_current_path->push_back(p);
	return old;
}

void PageWidget::finish_path()
{
	if (!m_current_path)
		return;
	m_view->undoStack->push(new AddStrokeCommand(m_view, m_view->current_page, 0, std::move(m_current_path)));
}
