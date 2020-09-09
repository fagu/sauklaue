#include "renderer.h"

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

void LayerPicture::setup_stroke(ptr_Stroke stroke) {
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
}

void LayerPicture::draw_stroke(int i)
{
	CairoGroup cg(cr);
	ptr_Stroke stroke = m_layer->strokes()[i];
	setup_stroke(stroke);
	PathStroke* path_stroke = convert_variant<PathStroke*>(stroke);
	draw_path(path_stroke->points());
}

void LayerPicture::draw_line(Point a, Point b, ptr_Stroke stroke)
{
	CairoGroup cg(cr);
	setup_stroke(stroke);
	cr->move_to(a.x, a.y);
	cr->line_to(b.x, b.y);
	double x1, y1, x2, y2;
	cr->get_stroke_extents(x1, y1, x2, y2);
	cr->stroke();
	updatePageRect(x1, y1, x2, y2);
}

void LayerPicture::updatePageRect(double x1, double y1, double x2, double y2)
{
	double minx = std::numeric_limits<double>::infinity(), maxx = -std::numeric_limits<double>::infinity(), miny = std::numeric_limits<double>::infinity(), maxy = -std::numeric_limits<double>::infinity();
	bounding_rect_helper(m_page_picture->page2widget, x1, y1, minx, maxx, miny, maxy);
	bounding_rect_helper(m_page_picture->page2widget, x1, y2, minx, maxx, miny, maxy);
	bounding_rect_helper(m_page_picture->page2widget, x2, y1, minx, maxx, miny, maxy);
	bounding_rect_helper(m_page_picture->page2widget, x2, y2, minx, maxx, miny, maxy);
	emit update(QRect(minx-1, miny-1, maxx-minx+3, maxy-miny+3));
}

PagePicture::PagePicture(Page* _page, int _width, int _height) :
	page(_page),
	width(_width),
	height(_height)
{
	assert(page);
	assert(width >= 20 && height >= 20);
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
	m_layers.emplace(m_layers.begin() + index, std::make_unique<LayerPicture>(page->layers()[index], this));
	connect(m_layers[index].get(), &LayerPicture::update, this, &PagePicture::update_layer);
}

void PagePicture::unregister_layer(int index)
{
	m_layers.erase(m_layers.begin() + index);
}

void PagePicture::update_layer(const QRect& rect)
{
	emit update(rect);
}

