#include "document.h"

#include "util.h"

#include "file1.pb.h"
#include "file3.pb.h"

#include "src/file4.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize-packed.h>

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

NormalLayer::NormalLayer(const NormalLayer& a)
{
	// Copy the strokes.
	for (const unique_ptr_Stroke &s : a.m_strokes) {
		m_strokes.emplace_back(std::visit(
			[](const auto& t) -> unique_ptr_Stroke {
				return std::make_unique<typename std::remove_reference_t<decltype(t)>::element_type>(*t);
			}, s));
	}
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

Page::Page(const Page& a) : Page(a.m_width, a.m_height)
{
	// Copy the layers.
	for (const std::unique_ptr<NormalLayer> &l : a.m_layers) {
		m_layers.emplace_back(std::make_unique<NormalLayer>(*l));
	}
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

Document::Document(const Document& a)
{
	// Copy the pages.
	for (const std::unique_ptr<Page> &p : a.pages) {
		pages.emplace_back(std::make_unique<Page>(*p));
	}
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

std::unique_ptr<Document> Document::concatenate(const std::vector<std::unique_ptr<Document> >& in_docs)
{
	std::unique_ptr<Document> res = std::make_unique<Document>();
	// Copy the pages.
	for (const std::unique_ptr<Document> &a : in_docs) {
		for (const std::unique_ptr<Page> &p : a->pages) {
			res->pages.emplace_back(std::make_unique<Page>(*p));
		}
	}
	return res;
}


const uint32_t FILE_FORMAT_VERSION = 4;

void write_path(file4::Path::Builder s_path, const std::vector<Point> &points) {
	auto s_points = s_path.initPoints(points.size());
	for (size_t i_point = 0; i_point < points.size(); i_point++) {
		const auto& point = points[i_point];
		auto s_point = s_points[i_point];
		s_point.setX(point.x);
		s_point.setY(point.y);
	}
}

void Document::save(QDataStream &stream)
{
	stream << FILE_FORMAT_VERSION;
	capnp::MallocMessageBuilder message;
	auto s_file = message.initRoot<file4::File>();
	auto s_pages = s_file.initPages(pages.size());
	for (size_t i_page = 0; i_page < pages.size(); i_page++) {
		const auto& page = pages[i_page];
		auto s_page = s_pages[i_page];
		s_page.setWidth(page->width());
		s_page.setHeight(page->height());
		auto s_layers = s_page.initLayers(page->layers().size());
		for (size_t i_layer = 0; i_layer < page->layers().size(); i_layer++) {
			const auto& layer = page->layers()[i_layer];
			auto s_layer = s_layers[i_layer];
			auto s_normal_layer = s_layer.initNormal();
			auto s_strokes = s_normal_layer.initStrokes(layer->strokes().size());
			for (size_t i_stroke = 0; i_stroke < layer->strokes().size(); i_stroke++) {
				const auto& stroke = layer->strokes()[i_stroke];
				auto s_stroke = s_strokes[i_stroke];
				std::visit(overloaded {
					[&](const PenStroke* st) {
						auto s_special_stroke = s_stroke.initPen();
						s_special_stroke.setWidth(st->width());
						s_special_stroke.setColor(st->color().x);
						auto s_path = s_special_stroke.initPath();
						write_path(s_path, st->points());
					},
					[&](const EraserStroke* st) {
						auto s_special_stroke = s_stroke.initEraser();
						s_special_stroke.setWidth(st->width());
						auto s_path = s_special_stroke.initPath();
						write_path(s_path, st->points());
					}
				}, get(stroke));
			}
		}
	}
	kj::VectorOutputStream out;
	capnp::writePackedMessage(out, message);
	stream.writeBytes(out.getArray().asChars().begin(), out.getArray().size());
}

void load_path_4(file4::Path::Reader s_path, PathStroke *path) {
	auto s_points = s_path.getPoints();
	assert(s_points.size() > 0);
	path->reserve_points(s_points.size());
	for (const auto& s_point : s_points) {
		Point point(s_point.getX(), s_point.getY());
		path->push_back(point);
	}
}

void load_path_3(file3::Path s_path, PathStroke *path) {
	assert(!s_path.points().empty());
	path->reserve_points(s_path.points_size());
	for (const auto& s_point : s_path.points()) {
		Point point(s_point.x(), s_point.y());
		path->push_back(point);
	}
}

void load_path_1(file1::Stroke s_path, PathStroke *path) {
	assert(!s_path.points().empty());
	path->reserve_points(s_path.points_size());
	for (const auto& s_point : s_path.points()) {
		Point point(s_point.x(), s_point.y());
		path->push_back(point);
	}
}

std::unique_ptr<Document> Document::load(QDataStream& stream)
{
	uint32_t file_format_version;
	stream >> file_format_version;
	assert(file_format_version <= FILE_FORMAT_VERSION);
	auto doc = std::make_unique<Document>();
	if (file_format_version >= 4) {
		char* c_data; uint len;
		stream.readBytes(c_data, len);
		kj::ArrayInputStream in(kj::arrayPtr((unsigned char*)c_data, len));
		// TODO Set the traversalLimitInWords to something other than 64MB. (Maybe just set it to a constant times the file size?)
		capnp::PackedMessageReader message(in);
		auto s_file = message.getRoot<file4::File>();
		for (auto s_page : s_file.getPages()) {
			auto page = std::make_unique<Page>(s_page.getWidth(), s_page.getHeight());
			for (auto s_layer : s_page.getLayers()) {
				auto s_normal_layer = s_layer.getNormal();
				auto layer = std::make_unique<NormalLayer>();
				auto s_strokes = s_normal_layer.getStrokes();
				layer->reserve_strokes(s_strokes.size());
				for (auto s_stroke : s_strokes) {
					unique_ptr_Stroke stroke;
					if (s_stroke.hasPen()) {
						auto s_special_stroke = s_stroke.getPen();
						auto special_stroke = std::make_unique<PenStroke>(s_special_stroke.getWidth(), s_special_stroke.getColor());
						load_path_4(s_special_stroke.getPath(), special_stroke.get());
						stroke = std::move(special_stroke);
					} else if (s_stroke.hasEraser()) {
						auto s_special_stroke = s_stroke.getEraser();
						auto special_stroke = std::make_unique<EraserStroke>(s_special_stroke.getWidth());
						load_path_4(s_special_stroke.getPath(), special_stroke.get());
						stroke = std::move(special_stroke);
					} else {
						assert(false);
					}
					layer->add_stroke(std::move(stroke));
				}
				page->add_layer(page->layers().size(), std::move(layer));
			}
			doc->add_page(doc->number_of_pages(), std::move(page));
		}
		delete[] c_data; // TODO Do this in an exception-safe way!
	} else {
		char* c_data; uint len;
		stream.readBytes(c_data, len);
		std::string data(c_data, len);
		delete[] c_data;
		google::protobuf::Arena arena;
		if (file_format_version >= 3) {
			file3::File *s_file = google::protobuf::Arena::CreateMessage<file3::File>(&arena);
			assert(s_file->ParseFromString(data));
			for (const auto& s_page : s_file->pages()) {
				auto page = std::make_unique<Page>(s_page.width(), s_page.height());
				for (const auto& s_layer : s_page.layers()) {
					const auto &s_normal_layer = s_layer.normal();
					auto layer = std::make_unique<NormalLayer>();
					layer->reserve_strokes(s_normal_layer.strokes_size());
					for (const auto& s_stroke : s_normal_layer.strokes()) {
						unique_ptr_Stroke stroke;
						if (s_stroke.has_pen()) {
							const auto &s_special_stroke = s_stroke.pen();
							Color color = s_special_stroke.color();
							auto special_stroke = std::make_unique<PenStroke>(s_special_stroke.width(), color);
							load_path_3(s_special_stroke.path(), special_stroke.get());
							stroke = std::move(special_stroke);
						} else if (s_stroke.has_eraser()) {
							const auto &s_special_stroke = s_stroke.eraser();
							auto special_stroke = std::make_unique<EraserStroke>(s_special_stroke.width());
							load_path_3(s_special_stroke.path(), special_stroke.get());
							stroke = std::move(special_stroke);
						} else {
							assert(false);
						}
						layer->add_stroke(std::move(stroke));
					}
					page->add_layer(page->layers().size(), std::move(layer));
				}
				doc->add_page(doc->number_of_pages(), std::move(page));
			}
		} else {
			file1::File *s_file = google::protobuf::Arena::CreateMessage<file1::File>(&arena);
			assert(s_file->ParseFromString(data));
			for (const auto& s_page : s_file->pages()) {
				auto page = std::make_unique<Page>(s_page.width(), s_page.height());
				for (const auto& s_layer : s_page.layers()) {
					const auto &s_normal_layer = s_layer.normal();
					auto layer = std::make_unique<NormalLayer>();
					for (const auto& s_stroke : s_normal_layer.strokes()) {
						Color color = file_format_version >= 2 ? s_stroke.color() : Color::BLACK;
						unique_ptr_Stroke stroke;
						if (color.a() > 0.5) { // Opaque => Assume PenStroke
							auto special_stroke = std::make_unique<PenStroke>(s_stroke.width(), color);
							load_path_1(s_stroke, special_stroke.get());
							stroke = std::move(special_stroke);
						} else {
							auto special_stroke = std::make_unique<EraserStroke>(s_stroke.width());
							load_path_1(s_stroke, special_stroke.get());
							stroke = std::move(special_stroke);
						}
						layer->add_stroke(std::move(stroke));
					}
					page->add_layer(page->layers().size(), std::move(layer));
				}
				doc->add_page(doc->number_of_pages(), std::move(page));
			}
		}
		qDebug() << "Arena space used:" << arena.SpaceUsed() << arena.SpaceAllocated();
	}
	qDebug() << "Read file";
	qDebug() << "Number of pages:" << doc->number_of_pages();
	int num_strokes = 0, num_points = 0;
	for (const auto &page : doc->pages) {
		for (const auto &layer : page->layers()) {
			for (const auto &stroke : layer->strokes()) {
				num_strokes++;
				std::visit(overloaded {
					[&](PathStroke* st) {
						num_points += st->points().size();
					}
				}, get(stroke));
			}
		}
	}
	qDebug() << "Number of strokes:" << num_strokes;
	qDebug() << "Number of points:" << num_points;
	return doc;
}
