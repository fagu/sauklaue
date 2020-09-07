#include "document.h"

#include "file.pb.h"

#include <cassert>
#include <iostream>

#include <QDebug>

const uint32_t Color::BLACK = Color::color(0,0,0,255);
const uint32_t Color::TRANSPARENT = Color::color(0,0,0,0);

const std::vector<Point> & Stroke::points() const
{
	return m_points;
}

Point Stroke::push_back(Point point)
{
	Point old = m_points.empty() ? point : m_points.back();
	m_points.push_back(point);
	return old;
}

NormalLayer::NormalLayer()
{
}

const std::vector<std::unique_ptr<Stroke> > & NormalLayer::strokes()
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
}

std::unique_ptr<Page> Document::delete_page(int index)
{
	assert(0 <= index && index < (int)pages.size());
	std::unique_ptr<Page> page = std::move(pages[index]);
	pages.erase(pages.begin() + index);
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

const uint32_t FILE_FORMAT_VERSION = 2;

void Document::save(QDataStream &stream)
{
	stream << FILE_FORMAT_VERSION;
	file::File s_file;
	for (const auto& page : pages) {
		file::Page *s_page = s_file.add_pages();
		s_page->set_width(page->width());
		s_page->set_height(page->height());
		for (const auto& layer : page->layers()) {
			file::Layer *s_layer = s_page->add_layers();
			file::NormalLayer *s_normal_layer = s_layer->mutable_normal();
			for (const auto& stroke : layer->strokes()) {
				file::Stroke *s_stroke = s_normal_layer->add_strokes();
				s_stroke->set_width(stroke->width());
				s_stroke->set_color(stroke->color());
				for (const auto& point : stroke->points()) {
					file::Point *s_point = s_stroke->add_points();
					s_point->set_x(point.x);
					s_point->set_y(point.y);
				}
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
	file::File s_file;
	assert(s_file.ParseFromString(data));
	auto doc = std::make_unique<Document>();
	for (const auto& s_page : s_file.pages()) {
		auto page = std::make_unique<Page>(s_page.width(), s_page.height());
		for (const auto& s_layer : s_page.layers()) {
			auto s_normal_layer = s_layer.normal();
			auto layer = std::make_unique<NormalLayer>();
			for (const auto& s_stroke : s_normal_layer.strokes()) {
				uint32_t color = file_format_version >= 2 ? s_stroke.color() : Color::BLACK;
				auto stroke = std::make_unique<Stroke>(s_stroke.width(), color);
				for (const auto& s_point : s_stroke.points()) {
					Point point(s_point.x(), s_point.y());
					stroke->push_back(point);
				}
				layer->add_stroke(std::move(stroke));
			}
			page->add_layer(page->layers().size(), std::move(layer));
		}
		doc->add_page(doc->number_of_pages(), std::move(page));
	}
	qDebug() << "Number of pages read: " << doc->number_of_pages();
	return doc;
}
