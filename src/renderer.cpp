#include "renderer.h"

#include <QDebug>

PictureTransformation::PictureTransformation(SPage* page, int widget_width, int widget_height)
{
	assert(page);
	assert(widget_width >= 20 && widget_width >= 20);
	int margin = 5;
	int rem_width = widget_width - 2*margin;
	int rem_height = widget_height - 2*margin;
	unit2pixel = std::min((double)rem_width/page->width(), (double)rem_height/page->height());
	image_size = QSize(unit2pixel*page->width(), unit2pixel*page->height());
	topLeft = QPoint((widget_width-image_size.width())/2, (widget_height-image_size.height())/2);
	image_rect = QRect(topLeft, image_size);
	page2image = Cairo::scaling_matrix(unit2pixel, unit2pixel);
	image2page = page2image;
	image2page.invert();
	page2widget = page2image * Cairo::translation_matrix(topLeft.x(), topLeft.y());
	qDebug() << "image_rect" << image_rect;
	qDebug() << "widget size" << QSize(widget_width, widget_height);
}


DrawingLayerPicture::DrawingLayerPicture(std::variant<NormalLayer*,TemporaryLayer*> layer, const PictureTransformation& transformation) :
	LayerPicture(transformation),
	m_layer(layer)
{
	connect(convert_variant<DrawingLayer*>(layer), &DrawingLayer::stroke_added, this, &DrawingLayerPicture::stroke_added);
	connect(convert_variant<DrawingLayer*>(layer), &DrawingLayer::stroke_deleted, this, &DrawingLayerPicture::stroke_deleted);
	
	cairo_surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, transformation.image_size.width(), transformation.image_size.height());
	cr = Cairo::Context::create(cairo_surface);
// 	cr->set_antialias(Cairo::ANTIALIAS_GRAY);
	cr->set_line_cap(Cairo::LINE_CAP_ROUND);
	cr->set_line_join(Cairo::LINE_JOIN_ROUND);
	cr->set_matrix(transformation.page2image);
	set_transparent();
// 	cr->rectangle(0,0,transformation.image_size.width(),transformation.image_size.height());
// 	cr->clip();
	draw_strokes();
}

void DrawingLayerPicture::set_transparent()
{
	CairoGroup cg(cr);
	cr->set_source_rgba(0,0,0,0);
	cr->set_operator(Cairo::OPERATOR_SOURCE);
	cr->paint(); // Reset every pixel to transparent.
}

void DrawingLayerPicture::draw_strokes()
{
	std::visit([&](auto layer) {
		for (ptr_Stroke stroke : layer->strokes())
			draw_stroke(stroke);
	}, m_layer);
	if (m_current_stroke)
		draw_stroke(m_current_stroke.value());
}

void construct_path(Cairo::RefPtr<Cairo::Context> cr, const std::vector<Point> &points) {
	assert(!points.empty());
	cr->move_to(points[0].x, points[0].y);
	if (points.size() == 1) {
		cr->line_to(points[0].x, points[0].y);
	} else {
		for (size_t i = 1; i < points.size(); i++)
			cr->line_to(points[i].x, points[i].y);
	}
}

void DrawingLayerPicture::stroke_added(ptr_Stroke stroke)
{
	if (m_current_stroke && m_current_stroke.value() == stroke)
		reset_current_stroke();
	else
		draw_stroke(stroke);
}

void DrawingLayerPicture::stroke_deleted(ptr_Stroke stroke)
{
// 	qDebug() << "Delete stroke" << m_layer->strokes().size();
	// TODO This redraws the entire page!
	// Since we know the stroke that is about to be deleted, only part of the page would need to be updated.
	{
		CairoGroup cg(cr);
		PathStroke* path_stroke = convert_variant<PathStroke*>(stroke);
		setup_stroke(stroke);
		construct_path(cr, path_stroke->points()); // Construct the deleted path to find the area that needs to be redrawn.
		QRect rect = stroke_extents();
		cr->begin_new_path(); // Forget the deleted path
		cr->set_matrix(Cairo::identity_matrix()); // Figure out the bounding rectangle in pixel coordinates
		cr->rectangle(rect.left(), rect.top(), rect.width(), rect.height());
		cr->clip();
		cr->set_matrix(transformation().page2image);
		set_transparent();
		draw_strokes();
	}
	emit update(QRect(0,0,transformation().image_size.width(),transformation().image_size.height()));
}

void DrawingLayerPicture::setup_stroke(ptr_Stroke stroke) {
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

void DrawingLayerPicture::draw_stroke(ptr_Stroke stroke)
{
	CairoGroup cg(cr);
	setup_stroke(stroke);
	PathStroke* path_stroke = convert_variant<PathStroke*>(stroke);
	construct_path(cr, path_stroke->points());
	// It might be better to compute the union of the extents of the individual segments.
	QRect rect = stroke_extents();
	cr->stroke();
	emit update(rect);
}

void DrawingLayerPicture::draw_line(Point a, Point b, ptr_Stroke stroke)
{
	CairoGroup cg(cr);
	setup_stroke(stroke);
	cr->move_to(a.x, a.y);
	cr->line_to(b.x, b.y);
	QRect rect = stroke_extents();
	cr->stroke();
	emit update(rect);
}

QRect DrawingLayerPicture::stroke_extents()
{
	double x1, y1, x2, y2;
	cr->get_stroke_extents(x1, y1, x2, y2);
	QRectF rect = bounding_rect(transformation().page2image, QRectF(QPointF(x1,y1), QPointF(x2,y2)));
	return QRect(QPoint((int)rect.left()-1, (int)rect.top()-1), QPoint((int)rect.right()+2, (int)rect.bottom()+2));
}



PDFLayerPicture::PDFLayerPicture(PDFLayer* layer, const PictureTransformation& transformation) :
	LayerPicture(transformation)
{
	m_layer = layer;
	m_layer->pdf()->document()->setRenderHint(Poppler::Document::RenderHint::Antialiasing);
	m_layer->pdf()->document()->setRenderHint(Poppler::Document::RenderHint::TextAntialiasing);
	// TODO Render asynchronously
	Poppler::Page* pdf_page = m_layer->page();
	double res = INCH_TO_UNIT * transformation.unit2pixel;
	m_img = pdf_page->renderToImage(res, res, 0, 0, transformation.image_size.width(), transformation.image_size.height());
	assert(!m_img.isNull());
}



PagePicture::PagePicture(SPage* _page, int _width, int _height) :
	page(_page),
	m_transformation(_page, _width, _height)
{
	for (size_t i = 0; i < page->layers().size(); i++)
		register_layer(i);
	
	connect(page, &SPage::layer_added, this, &PagePicture::register_layer);
	connect(page, &SPage::layer_deleted, this, &PagePicture::unregister_layer);
	
	m_temporary_layer = std::make_unique<DrawingLayerPicture>(page->temporary_layer(), transformation());
	connect(m_temporary_layer.get(), &DrawingLayerPicture::update, this, &PagePicture::update_layer);
}

void PagePicture::register_layer(int index)
{
	ptr_Layer layer = page->layers()[index];
	unique_ptr_LayerPicture pic = std::visit(overloaded {
		[&](NormalLayer* layer) -> unique_ptr_LayerPicture {
			return std::make_unique<DrawingLayerPicture>(layer, transformation());
		},
		[&](PDFLayer* layer) -> unique_ptr_LayerPicture {
			return std::make_unique<PDFLayerPicture>(layer, transformation());
		}
	}, layer);
	ptr_LayerPicture p_pic = get(pic);
	m_layers.emplace(m_layers.begin() + index, std::move(pic));
	connect(convert_variant<LayerPicture*>(p_pic), &LayerPicture::update, this, &PagePicture::update_layer);
}

void PagePicture::unregister_layer(int index)
{
	emit removing_layer(get(m_layers[index]));
	m_layers.erase(m_layers.begin() + index);
}

void PagePicture::update_layer(const QRect& rect)
{
	emit update(rect);
}

void PDFExporter::save(Document* doc, const std::string& file_name)
{
	std::map<EmbeddedPDF*,GObjectWrapper<PopplerDocument> > poppler_docs;
	for (EmbeddedPDF* pdf : doc->embedded_pdfs())
		poppler_docs.emplace(pdf, pdf->glib_document());
	Cairo::RefPtr<Cairo::PdfSurface> surface = Cairo::PdfSurface::create(file_name, 0, 0);
	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surface);
	cr->set_line_cap(Cairo::LINE_CAP_ROUND);
	cr->set_line_join(Cairo::LINE_JOIN_ROUND);
	cr->scale(UNIT_TO_POINT, UNIT_TO_POINT);
	for (SPage *page : doc->pages()) {
		// Decide automatically whether to use simplistic mode:
		// It's safe to use whenever the background is white and there are no eraser strokes on any layer except layer 0.
		bool simplistic = true;
		bool first_layer = true;
		for (auto layer : page->layers()) {
			if (first_layer) {
				first_layer = false;
			} else {
				std::visit(overloaded {
					[&](NormalLayer* layer) {
						for (auto stroke : layer->strokes()) {
							if (std::holds_alternative<EraserStroke*>(stroke))
								simplistic = false;
						}
					},
					[&](PDFLayer*) {
					}
				}, layer);
			}
		}
		qDebug() << "Drawing mode:" << (simplistic ? "simplistic" : "general");
		surface->set_size(UNIT_TO_POINT*page->width(), UNIT_TO_POINT*page->height());
		cr->rectangle(0,0,page->width(),page->height());
		cr->clip();
		if (!simplistic) {
			for ([[maybe_unused]] auto layer : page->layers())
				cr->push_group_with_content(Cairo::CONTENT_COLOR_ALPHA);
			cr->set_source_rgb(1,1,1);
			cr->paint();
		}
		for (auto layer : page->layers()) {
			// Retrieve and draw the previous layers
			Cairo::RefPtr<Cairo::Pattern> background;
			if (!simplistic) {
				background = cr->pop_group();
				cr->set_source(background);
				cr->paint();
			}
			std::visit(overloaded {
				[&](NormalLayer* layer) {
					// TODO Find a better way to support the eraser.
					// The operator CAIRO_OPERATOR_SOURCE is apparently not supported by PDF files. Therefore Cairo falls back to saving a raster image in the PDF file, which uses a lot of space!
		// 			cairo_set_operator(cr->cobj(), CAIRO_OPERATOR_SOURCE);
					for (auto stroke : layer->strokes()) {
						std::visit(overloaded {
							[&](PenStroke* st) {
								cr->set_line_width(st->width());
								Color co = st->color();
								cr->set_source_rgba(co.r(), co.g(), co.b(), co.a());
								construct_path(cr, st->points());
								cr->stroke();
							},
							[&](EraserStroke* st) {
								cr->set_line_width(st->width());
								if (!simplistic) {
									// TODO This seems to slow down the PDF viewer.
									cr->set_source(background); // Erase = draw the background again on top of this layer
								} else {
									cr->set_source_rgb(1,1,1);
								}
								construct_path(cr, st->points());
								cr->stroke();
							}
						}, stroke);
					}
				},
				[&](PDFLayer* layer) {
					GObjectWrapper<PopplerPage> page(poppler_document_get_page(poppler_docs[layer->pdf()].get(), layer->page_number()));
					if (!page)
						throw PDFReadException("Invalid page " + QString::number(layer->page_number()+1));
					CairoGroup cg(cr);
					cr->scale(POINT_TO_UNIT, POINT_TO_UNIT); // Use original scale
					poppler_page_render(page.get(), cr->cobj());
				}
			}, layer);
		}
		surface->show_page();
	}
}
