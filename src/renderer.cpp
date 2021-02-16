#include "renderer.h"

#include "cairo-helpers.h"
#include "all-types.h"
#include "document.h"

#include <QDebug>

// The header poppler.h defines a variable called signals, which is a qt keyword.
#undef signals
#include <poppler.h>
#define signals Q_SIGNALS

PictureTransformation::PictureTransformation(SPage* page, int widget_width, int widget_height) {
	assert(page);
	assert(widget_width >= 20 && widget_width >= 20);
	int margin = 5;
	int rem_width = widget_width - 2 * margin;
	int rem_height = widget_height - 2 * margin;
	unit2pixel = std::min((double)rem_width / page->width(), (double)rem_height / page->height());
	image_size = QSize(unit2pixel * page->width(), unit2pixel * page->height());
	topLeft = QPoint((widget_width - image_size.width()) / 2, (widget_height - image_size.height()) / 2);
	image_rect = QRect(topLeft, image_size);
}

// We do not use Cairo's transformation matrix because I don't understand what it does when used together with get_stroke_extents.
// We really want to know the bounding rectangle to be updated in image coordinates. But we get strange results when resetting the transformation matrix to the identity matrix between constructing the path and calling get_stroke_extents.
void construct_path(Cairo::RefPtr<Cairo::Context> cr, const std::vector<Point>& points, double unit2pixel) {
	assert(!points.empty());
	cr->move_to(points[0].x * unit2pixel, points[0].y * unit2pixel);
	if (points.size() == 1) {
		cr->line_to(points[0].x * unit2pixel, points[0].y * unit2pixel);
	} else {
		for (size_t i = 1; i < points.size(); i++)
			cr->line_to(points[i].x * unit2pixel, points[i].y * unit2pixel);
	}
}

Renderer::Renderer(const PictureTransformation& transformation) :
    m_transformation(transformation) {
	cairo_surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, transformation.image_size.width(), transformation.image_size.height());
	cr = Cairo::Context::create(cairo_surface);
	// 	cr->set_antialias(Cairo::ANTIALIAS_GRAY);
	cr->set_line_cap(Cairo::LINE_CAP_ROUND);
	cr->set_line_join(Cairo::LINE_JOIN_ROUND);
	set_transparent();
}

void Renderer::set_transparent(std::optional<QRect> rect) {
	CairoGroup cg(cr);
	if (rect) {
		cr->rectangle(rect->left(), rect->top(), rect->width(), rect->height());
		cr->clip();
	}
	cr->set_source_rgba(0, 0, 0, 0);
	cr->set_operator(Cairo::OPERATOR_SOURCE);
	cr->paint();
}

QImage Renderer::img() const {
	cairo_surface->flush();
	return QImage((const uchar*)cairo_surface->get_data(), cairo_surface->get_width(), cairo_surface->get_height(), QImage::Format_ARGB32_Premultiplied);
}

void Renderer::copy_from(const Renderer& other_renderer, std::optional<QRect> rect) {
	Cairo::RefPtr<Cairo::ImageSurface> other_surface = other_renderer.cairo_surface;
	assert(cairo_surface->get_format() == Cairo::FORMAT_ARGB32);
	assert(other_surface->get_format() == Cairo::FORMAT_ARGB32);
	assert(other_surface->get_width() == cairo_surface->get_width());
	assert(other_surface->get_height() == cairo_surface->get_height());
	assert(other_surface->get_stride() == cairo_surface->get_stride());
	cairo_surface->flush();
	other_surface->flush();
	QRect r = rect ? rect.value() : QRect(0, 0, cairo_surface->get_width(), cairo_surface->get_height());
	int x1 = std::max(0, r.left());
	int x2 = std::min(cairo_surface->get_width() - 1, r.right());
	int y1 = std::max(0, r.top());
	int y2 = std::min(cairo_surface->get_height() - 1, r.bottom());
	unsigned char* this_data = cairo_surface->get_data();
	const unsigned char* other_data = other_surface->get_data();
	int stride = cairo_surface->get_stride();
	if (y1 <= y2 && x1 <= x2) {
		for (int y = y1; y <= y2; y++) {
			size_t offset = (size_t)y * stride + 4 * x1;
			memcpy(this_data + offset, other_data + offset, 4 * (x2 - x1 + 1));
		}
		cairo_surface->mark_dirty(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
	}
}

QRect Renderer::draw_stroke(ptr_Stroke stroke, std::optional<QRect> clip_rect) {
	CairoGroup cg(cr);
	if (clip_rect) {
		cr->rectangle(clip_rect->left(), clip_rect->top(), clip_rect->width(), clip_rect->height());
		cr->clip();
	}
	setup_stroke(stroke);
	QRect rect = current_stroke_extents();
	cr->stroke();
	return rect;
}

QRect Renderer::stroke_extents(ptr_Stroke stroke) {
	CairoGroup cg(cr);
	setup_stroke(stroke);
	QRect rect = current_stroke_extents();
	cr->begin_new_path();
	return rect;
}

void Renderer::setup_stroke(ptr_Stroke stroke) {
	double unit2pixel = m_transformation.unit2pixel;
	std::visit(overloaded{[&](const PenStroke* st) {
		                      cr->set_line_width(st->width() * unit2pixel);
		                      Color co = st->color();
		                      cr->set_source_rgba(co.r(), co.g(), co.b(), co.a());
	                      },
	                      [&](const EraserStroke* st) {
		                      cr->set_line_width(st->width() * unit2pixel);
		                      cr->set_source_rgba(0, 0, 0, 0);  // Transparent
		                      // Replace layer color by transparent instead of making a transparent drawing on top of the layer.
		                      cr->set_operator(Cairo::OPERATOR_SOURCE);
	                      }},
	           stroke);
	PathStroke* path_stroke = convert_variant<PathStroke*>(stroke);
	construct_path(cr, path_stroke->points(), unit2pixel);
}

QRect Renderer::current_stroke_extents() {
	double x1, y1, x2, y2;
	cr->get_stroke_extents(x1, y1, x2, y2);
	return QRect(QPoint((int)x1 - 1, (int)y1 - 1), QPoint((int)x2 + 2, (int)y2 + 2));
}

DrawingLayerPicture::DrawingLayerPicture(std::variant<NormalLayer*, TemporaryLayer*> layer, const PictureTransformation& transformation) :
    LayerPicture(transformation),
    committed_strokes(transformation),
    all_strokes(transformation),
    m_layer(layer) {
	connect(convert_variant<DrawingLayer*>(layer), &DrawingLayer::stroke_added, this, &DrawingLayerPicture::stroke_added);
	connect(convert_variant<DrawingLayer*>(layer), &DrawingLayer::stroke_deleted, this, &DrawingLayerPicture::stroke_deleted);

	committed_strokes.set_transparent();
	redraw();
}

void DrawingLayerPicture::set_current_stroke(ptr_Stroke current_stroke) {
	if (m_current_stroke != current_stroke) {
		m_current_stroke = current_stroke;
		redraw_current();
	}
}

void DrawingLayerPicture::reset_current_stroke() {
	if (m_current_stroke) {
		m_current_stroke.reset();
		redraw_current();
	}
}

void DrawingLayerPicture::redraw(std::optional<QRect> rect) {
	committed_strokes.set_transparent(rect);
	std::visit([&](auto layer) {
		for (ptr_Stroke stroke : layer->strokes())
			committed_strokes.draw_stroke(stroke, rect);
	},
	           m_layer);
	redraw_current(rect);
}

void DrawingLayerPicture::redraw_current(std::optional<QRect> rect) {
	all_strokes.copy_from(committed_strokes, rect);
	if (m_current_stroke)
		all_strokes.draw_stroke(m_current_stroke.value(), rect);
}

void DrawingLayerPicture::stroke_added(ptr_Stroke stroke) {
	if (m_current_stroke && m_current_stroke.value() == stroke)
		reset_current_stroke();
	QRect rect = committed_strokes.draw_stroke(stroke);
	all_strokes.copy_from(committed_strokes, rect);
	if (m_current_stroke)
		all_strokes.draw_stroke(m_current_stroke.value(), rect);
	emit update(rect);
}

void DrawingLayerPicture::stroke_deleted(ptr_Stroke stroke) {
	QRect rect = committed_strokes.stroke_extents(stroke);
	redraw(rect);
	emit update(rect);
}

void DrawingLayerPicture::draw_line(Point a, Point b, ptr_Stroke stroke) {
	ptr_Stroke final_line = std::visit(overloaded{[&](const PenStroke* st) -> ptr_Stroke {
		                                              PenStroke* n = new PenStroke(st->width(), st->color());
		                                              n->push_back(a);
		                                              n->push_back(b);
		                                              return n;
	                                              },
	                                              [&](const EraserStroke* st) -> ptr_Stroke {
		                                              EraserStroke* n = new EraserStroke(st->width());
		                                              n->push_back(a);
		                                              n->push_back(b);
		                                              return n;
	                                              }},
	                                   stroke);
	QRect rect = all_strokes.stroke_extents(final_line);
	redraw_current(rect);
	emit update(rect);
}

PDFLayerPicture::PDFLayerPicture(PDFLayer* layer, const PictureTransformation& transformation) :
    LayerPicture(transformation) {
	m_layer = layer;
	connect(m_layer, &PDFLayer::changed, this, &PDFLayerPicture::redraw);
	redraw();
}

void PDFLayerPicture::redraw() {
	cairo_surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, transformation().image_size.width(), transformation().image_size.height());
	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(cairo_surface);
	cr->set_antialias(Cairo::ANTIALIAS_GRAY);
	Cairo::FontOptions font_options;
	font_options.set_antialias(Cairo::ANTIALIAS_GRAY);
	cr->set_font_options(font_options);
	double scale = POINT_TO_UNIT * transformation().unit2pixel;
	cr->scale(scale, scale);
	// TODO Render asynchronously
	poppler_page_render(m_layer->page(), cr->cobj());
	emit update(QRect(QPoint(0, 0), transformation().image_size));
}

PagePicture::PagePicture(SPage* _page, int _width, int _height) :
    page(_page),
    m_transformation(_page, _width, _height) {
	for (size_t i = 0; i < page->layers().size(); i++)
		register_layer(i);

	connect(page, &SPage::layer_added, this, &PagePicture::register_layer);
	connect(page, &SPage::layer_deleted, this, &PagePicture::unregister_layer);

	m_temporary_layer = std::make_unique<DrawingLayerPicture>(page->temporary_layer(), transformation());
	connect(m_temporary_layer.get(), &DrawingLayerPicture::update, this, &PagePicture::update_layer);
}

void PagePicture::register_layer(int index) {
	ptr_Layer layer = page->layers()[index];
	unique_ptr_LayerPicture pic = std::visit(overloaded{[&](NormalLayer* layer) -> unique_ptr_LayerPicture {
		                                                    return std::make_unique<DrawingLayerPicture>(layer, transformation());
	                                                    },
	                                                    [&](PDFLayer* layer) -> unique_ptr_LayerPicture {
		                                                    return std::make_unique<PDFLayerPicture>(layer, transformation());
	                                                    }},
	                                         layer);
	ptr_LayerPicture p_pic = get(pic);
	m_layers.emplace(m_layers.begin() + index, std::move(pic));
	connect(convert_variant<LayerPicture*>(p_pic), &LayerPicture::update, this, &PagePicture::update_layer);
}

void PagePicture::unregister_layer(int index) {
	emit removing_layer(get(m_layers[index]));
	m_layers.erase(m_layers.begin() + index);
}

void PagePicture::update_layer(const QRect& rect) {
	emit update(rect);
}

void PDFExporter::save(Document* doc, const std::string& file_name) {
	Cairo::RefPtr<Cairo::PdfSurface> surface = Cairo::PdfSurface::create(file_name, 0, 0);
	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surface);
	cr->set_line_cap(Cairo::LINE_CAP_ROUND);
	cr->set_line_join(Cairo::LINE_JOIN_ROUND);
	cr->scale(UNIT_TO_POINT, UNIT_TO_POINT);
	for (SPage* page : doc->pages()) {
		// Decide automatically whether to use simplistic mode:
		// It's safe to use whenever the background is white and there are no eraser strokes on any layer except layer 0.
		bool simplistic = true;
		bool first_layer = true;
		for (auto layer : page->layers()) {
			if (first_layer) {
				first_layer = false;
			} else {
				std::visit(overloaded{[&](NormalLayer* layer) {
					                      for (auto stroke : layer->strokes()) {
						                      if (std::holds_alternative<EraserStroke*>(stroke))
							                      simplistic = false;
					                      }
				                      },
				                      [&](PDFLayer*) {
				                      }},
				           layer);
			}
		}
		qDebug() << "Drawing mode:" << (simplistic ? "simplistic" : "general");
		surface->set_size(UNIT_TO_POINT * page->width(), UNIT_TO_POINT * page->height());
		cr->rectangle(0, 0, page->width(), page->height());
		cr->clip();
		if (!simplistic) {
			for ([[maybe_unused]] auto layer : page->layers())
				cr->push_group_with_content(Cairo::CONTENT_COLOR_ALPHA);
			cr->set_source_rgb(1, 1, 1);
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
			std::visit(overloaded{[&](NormalLayer* layer) {
				                      // TODO Find a better way to support the eraser.
				                      // The operator CAIRO_OPERATOR_SOURCE is apparently not supported by PDF files. Therefore Cairo falls back to saving a raster image in the PDF file, which uses a lot of space!
				                      // 			cairo_set_operator(cr->cobj(), CAIRO_OPERATOR_SOURCE);
				                      for (auto stroke : layer->strokes()) {
					                      std::visit(overloaded{[&](PenStroke* st) {
						                                            cr->set_line_width(st->width());
						                                            Color co = st->color();
						                                            cr->set_source_rgba(co.r(), co.g(), co.b(), co.a());
						                                            construct_path(cr, st->points(), 1.0);
						                                            cr->stroke();
					                                            },
					                                            [&](EraserStroke* st) {
						                                            cr->set_line_width(st->width());
						                                            if (!simplistic) {
							                                            // TODO This seems to slow down the PDF viewer.
							                                            cr->set_source(background);  // Erase = draw the background again on top of this layer
						                                            } else {
							                                            cr->set_source_rgb(1, 1, 1);
						                                            }
						                                            construct_path(cr, st->points(), 1.0);
						                                            cr->stroke();
					                                            }},
					                                 stroke);
				                      }
			                      },
			                      [&](PDFLayer* layer) {
				                      CairoGroup cg(cr);
				                      cr->scale(POINT_TO_UNIT, POINT_TO_UNIT);  // Use original scale
				                      poppler_page_render(layer->page(), cr->cobj());
			                      }},
			           layer);
		}
		surface->show_page();
	}
}
