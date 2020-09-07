#include "document.h"

#include "util.h"

#include "file1.pb.h"
#include "file3.pb.h"

#include <cassert>
#include <iostream>

#include <QDebug>

const Color Color::BLACK = Color::color(0,0,0,255);

const std::vector<Point> & PathStroke::points() const
{
	return m_points;
}

void PathStroke::push_back(Point point)
{
	m_points.push_back(point);
}

NormalLayer::NormalLayer()
{
}

const std::vector<unique_ptr_Stroke> & NormalLayer::strokes()
{
	return m_strokes;
}


Page::Page(int w, int h) :
	m_width(w),
	m_height(h)
{
}

int Page::width()
{
	return m_width;
}

int Page::height()
{
	return m_height;
}


Document::Document()
{

}

void Document::add_page(int at, std::unique_ptr<Page> page)
{
	assert(0 <= at && at <= (int)pages.size());
	pages.insert(pages.begin() + at, std::move(page));
	emit page_added(at);
}

std::unique_ptr<Page> Document::delete_page(int index)
{
	assert(0 <= index && index < (int)pages.size());
	std::unique_ptr<Page> page = std::move(pages[index]);
	pages.erase(pages.begin() + index);
	emit page_deleted(index);
	return page;
}

Page * Document::page(int index)
{
	assert(0 <= index && index < (int)pages.size());
	return pages[index].get();
}

int Document::number_of_pages()
{
	return pages.size();
}

const uint32_t FILE_FORMAT_VERSION = 3;

void write_path(file3::Path *s_path, const std::vector<Point> &points) {
	for (const auto& point : points) {
		file3::Point *s_point = s_path->add_points();
		s_point->set_x(point.x);
		s_point->set_y(point.y);
	}
}

void Document::save(QDataStream &stream)
{
	stream << FILE_FORMAT_VERSION;
	file3::File s_file;
	for (const auto& page : pages) {
		file3::Page *s_page = s_file.add_pages();
		s_page->set_width(page->width());
		s_page->set_height(page->height());
		for (const auto& layer : page->layers()) {
			file3::Layer *s_layer = s_page->add_layers();
			file3::NormalLayer *s_normal_layer = s_layer->mutable_normal();
			for (const auto& stroke : layer->strokes()) {
				file3::Stroke *s_stroke = s_normal_layer->add_strokes();
				std::visit(overloaded {
					[&](const PenStroke* st) {
						file3::PenStroke *s_special_stroke = s_stroke->mutable_pen();
						s_special_stroke->set_width(st->width());
						s_special_stroke->set_color(st->color().x);
						file3::Path *s_path = s_special_stroke->mutable_path();
						write_path(s_path, st->points());
					},
					[&](const EraserStroke* st) {
						file3::EraserStroke *s_special_stroke = s_stroke->mutable_eraser();
						s_special_stroke->set_width(st->width());
						file3::Path *s_path = s_special_stroke->mutable_path();
						write_path(s_path, st->points());
					}
				}, get(stroke));
			}
		}
	}
	std::string data = s_file.SerializeAsString();
	stream.writeBytes(data.c_str(), data.length());
}

std::unique_ptr<Document> Document::load(QDataStream& stream)
{
	uint32_t file_format_version;
	stream >> file_format_version;
	assert(file_format_version <= FILE_FORMAT_VERSION);
	char* c_data; uint len;
	stream.readBytes(c_data, len);
	std::string data(c_data, len);
	delete[] c_data;
	auto doc = std::make_unique<Document>();
	if (file_format_version >= 3) {
		file3::File s_file;
		assert(s_file.ParseFromString(data));
		for (const auto& s_page : s_file.pages()) {
			auto page = std::make_unique<Page>(s_page.width(), s_page.height());
			for (const auto& s_layer : s_page.layers()) {
				const auto &s_normal_layer = s_layer.normal();
				auto layer = std::make_unique<NormalLayer>();
				for (const auto& s_stroke : s_normal_layer.strokes()) {
					unique_ptr_Stroke stroke;
					if (s_stroke.has_pen()) {
						const auto &s_special_stroke = s_stroke.pen();
						Color color = s_special_stroke.color();
						auto special_stroke = std::make_unique<PenStroke>(s_special_stroke.width(), color);
						assert(!s_special_stroke.path().points().empty());
						for (const auto& s_point : s_special_stroke.path().points()) {
							Point point(s_point.x(), s_point.y());
							special_stroke->push_back(point);
						}
						stroke = std::move(special_stroke);
					} else if (s_stroke.has_eraser()) {
						const auto &s_special_stroke = s_stroke.eraser();
						auto special_stroke = std::make_unique<EraserStroke>(s_special_stroke.width());
						assert(!s_special_stroke.path().points().empty());
						for (const auto& s_point : s_special_stroke.path().points()) {
							Point point(s_point.x(), s_point.y());
							special_stroke->push_back(point);
						}
						stroke = std::move(special_stroke);
					}
					layer->add_stroke(std::move(stroke));
				}
				page->add_layer(page->layers().size(), std::move(layer));
			}
			doc->add_page(doc->number_of_pages(), std::move(page));
		}
	} else {
		file1::File s_file;
		assert(s_file.ParseFromString(data));
		for (const auto& s_page : s_file.pages()) {
			auto page = std::make_unique<Page>(s_page.width(), s_page.height());
			for (const auto& s_layer : s_page.layers()) {
				const auto &s_normal_layer = s_layer.normal();
				auto layer = std::make_unique<NormalLayer>();
				for (const auto& s_stroke : s_normal_layer.strokes()) {
					Color color = file_format_version >= 2 ? s_stroke.color() : Color::BLACK;
					unique_ptr_Stroke stroke;
					if (color.a() > 0.5) { // Opaque => Assume PenStroke
						stroke = std::make_unique<PenStroke>(s_stroke.width(), color);
					} else {
						stroke = std::make_unique<EraserStroke>(s_stroke.width());
					}
					assert(!s_stroke.points().empty());
					for (const auto& s_point : s_stroke.points()) {
						Point point(s_point.x(), s_point.y());
						std::visit(overloaded {
							[&](PenStroke* st) {
								st->push_back(point);
							},
							[&](EraserStroke* st) {
								st->push_back(point);
							}
						}, get(stroke));
					}
					layer->add_stroke(std::move(stroke));
				}
				page->add_layer(page->layers().size(), std::move(layer));
			}
			doc->add_page(doc->number_of_pages(), std::move(page));
		}
	}
	qDebug() << "Number of pages read: " << doc->number_of_pages();
	return doc;
}
