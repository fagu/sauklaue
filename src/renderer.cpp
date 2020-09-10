#include "renderer.h"

#include <QDebug>

LayerPicture::LayerPicture(NormalLayer* layer, PagePicture* page_picture) :
	m_layer(layer),
	m_page_picture(page_picture)
{
	connect(layer, &NormalLayer::stroke_added, this, &LayerPicture::stroke_added);
	connect(layer, &NormalLayer::stroke_deleted, this, &LayerPicture::stroke_deleted);
	
	cairo_surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, m_page_picture->width, m_page_picture->height);
	cr = Cairo::Context::create(cairo_surface);
// 	cr->set_antialias(Cairo::ANTIALIAS_GRAY);
	cr->set_line_cap(Cairo::LINE_CAP_ROUND);
	cr->set_line_join(Cairo::LINE_JOIN_ROUND);
	cr->save();
	setup();
	for (ptr_Stroke stroke : m_layer->strokes()) // Don't draw the last stroke!
		draw_stroke(stroke);
	for (ptr_Stroke stroke : m_layer->temporary_strokes()) // Don't draw the last stroke!
		draw_stroke(stroke);
}

void LayerPicture::stroke_added(ptr_Stroke stroke)
{
	draw_stroke(stroke);
}

void LayerPicture::stroke_deleted(ptr_Stroke stroke)
{
	// TODO This redraws the entire page!
	// Since we know the stroke that is about to be deleted, only part of the page would need to be updated.
	setup();
	for (ptr_Stroke stroke : m_layer->strokes()) // Don't draw the last stroke!
		draw_stroke(stroke);
	for (ptr_Stroke stroke : m_layer->temporary_strokes()) // Don't draw the last stroke!
		draw_stroke(stroke);
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

void draw_path(Cairo::RefPtr<Cairo::Context> cr, const std::vector<Point> &points) {
	assert(!points.empty());
	cr->move_to(points[0].x, points[0].y);
	if (points.size() == 1) {
		cr->line_to(points[0].x, points[0].y);
	} else {
		for (size_t i = 1; i < points.size(); i++)
			cr->line_to(points[i].x, points[i].y);
	}
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

void LayerPicture::draw_stroke(ptr_Stroke stroke)
{
	CairoGroup cg(cr);
	setup_stroke(stroke);
	PathStroke* path_stroke = convert_variant<PathStroke*>(stroke);
	draw_path(cr, path_stroke->points());
	// It might be better to compute the union of the extents of the individual segments.
	double x1, y1, x2, y2;
	cr->get_stroke_extents(x1, y1, x2, y2);
	cr->stroke();
	updatePageRect(x1, y1, x2, y2);
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
	QRectF rect = bounding_rect(m_page_picture->page2widget, QRectF(QPointF(x1,y1), QPointF(x2,y2)));
	emit update(QRect(QPoint((int)rect.left()-1, (int)rect.top()-1), QPoint((int)rect.right()+2, (int)rect.bottom()+2)));
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



void PDFExporter::save(Document* doc, const std::string& file_name)
{
	Cairo::RefPtr<Cairo::PdfSurface> surface = Cairo::PdfSurface::create(file_name, 0, 0);
	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surface);
	cr->set_line_cap(Cairo::LINE_CAP_ROUND);
	cr->set_line_join(Cairo::LINE_JOIN_ROUND);
	const double SCALE = 0.001;
	cr->scale(SCALE, SCALE);
	for (Page *page : doc->pages()) {
		surface->set_size(SCALE*page->width(), SCALE*page->height());
		cr->rectangle(0,0,page->width(),page->height());
		cr->clip();
		for (auto layer : page->layers())
			cr->push_group_with_content(Cairo::CONTENT_COLOR_ALPHA);
		cr->set_source_rgb(1,1,1);
		cr->paint();
		for (auto layer : page->layers()) {
			// Retrieve and draw the previous layers
			Cairo::RefPtr<Cairo::Pattern> background = cr->pop_group();
			cr->set_source(background);
			cr->paint();
			// TODO Find a better way to support the eraser.
			// The operator CAIRO_OPERATOR_SOURCE is apparently not supported by PDF files. Therefore Cairo falls back to saving a raster image in the PDF file, which uses a lot of space!
// 			cairo_set_operator(cr->cobj(), CAIRO_OPERATOR_SOURCE);
			for (auto stroke : layer->strokes()) {
				std::visit(overloaded {
					[&](PenStroke* st) {
						cr->set_line_width(st->width());
						Color co = st->color();
						cr->set_source_rgba(co.r(), co.g(), co.b(), co.a());
						draw_path(cr, st->points());
						cr->stroke();
					},
					[&](EraserStroke* st) {
						cr->set_line_width(st->width());
						// TODO This seems to slow down the PDF viewer.
						cr->set_source(background); // Erase = draw the background again on top of this layer
						draw_path(cr, st->points());
						cr->stroke();
					}
				}, stroke);
			}
		}
		surface->show_page();
	}
}
