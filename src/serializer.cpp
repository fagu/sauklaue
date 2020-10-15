#include "serializer.h"

#include "document.h"

#include "src/file4.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize-packed.h>

#include <iostream>

#include <QDebug>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QDataStream>

const uint32_t FILE_FORMAT_VERSION = 5;
constexpr std::string_view magic_string("sauklaue_9NyB3wiHcGwA1dPGoadQJry");

void write_path(file4::Path::Builder s_path, const std::vector<Point>& points) {
	auto s_points = s_path.initPoints(points.size());
	for (size_t i_point = 0; i_point < points.size(); i_point++) {
		auto point = points[i_point];
		auto s_point = s_points[i_point];
		s_point.setX(point.x);
		s_point.setY(point.y);
	}
}

void Serializer::save(Document* doc, QDataStream& stream) {
	/*QElapsedTimer copy_timer; copy_timer.start();
	auto cop = std::make_unique<Document>(*this);
	qDebug() << "copy" << copy_timer.elapsed();
	QElapsedTimer allocate_timer; allocate_timer.start();
	std::vector<unique_ptr_Stroke> strokes;
	for (size_t i = 0; i < 75000; i++)
		strokes.emplace_back(std::make_unique<PenStroke>(0, Color::BLACK));
	qDebug() << "allocate" << allocate_timer.elapsed();*/
	stream.writeRawData(magic_string.data(), magic_string.size());
	stream << FILE_FORMAT_VERSION;
	stream.setVersion(QDataStream::Qt_5_15);
	capnp::MallocMessageBuilder message;
	QElapsedTimer construct_timer;
	construct_timer.start();
	auto s_file = message.initRoot<file4::File>();
	auto s_pdfs = s_file.initEmbeddedPDFs(doc->embedded_pdfs().size());
	std::map<EmbeddedPDF*, int> embedded_pdf_index;
	auto pdf_iterator = doc->embedded_pdfs().begin();
	for (size_t i_pdf = 0; i_pdf < doc->embedded_pdfs().size(); i_pdf++) {
		auto pdf = *pdf_iterator;
		auto s_pdf = s_pdfs[i_pdf];
		int index = embedded_pdf_index.size();
		embedded_pdf_index[pdf] = index;
		s_pdf.setName(pdf->name().toStdString());
		QByteArray contents = pdf->contents();
		s_pdf.setContents(kj::arrayPtr((const unsigned char*)contents.constData(), contents.size()));
		++pdf_iterator;
	}
	auto s_pages = s_file.initPages(doc->pages().size());
	for (size_t i_page = 0; i_page < doc->pages().size(); i_page++) {
		auto page = doc->pages()[i_page];
		auto s_page = s_pages[i_page];
		s_page.setWidth(page->width());
		s_page.setHeight(page->height());
		auto s_layers = s_page.initLayers(page->layers().size());
		for (size_t i_layer = 0; i_layer < page->layers().size(); i_layer++) {
			auto layer = page->layers()[i_layer];
			auto s_layer = s_layers[i_layer];
			std::visit(overloaded{[&](NormalLayer* layer) {
				                      auto s_normal_layer = s_layer.initNormal();
				                      auto s_strokes = s_normal_layer.initStrokes(layer->strokes().size());
				                      for (size_t i_stroke = 0; i_stroke < layer->strokes().size(); i_stroke++) {
					                      auto stroke = layer->strokes()[i_stroke];
					                      auto s_stroke = s_strokes[i_stroke];
					                      std::visit(overloaded{[&](const PenStroke* st) {
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
					                                            }},
					                                 stroke);
				                      }
			                      },
			                      [&](PDFLayer* layer) {
				                      auto s_pdf_layer = s_layer.initPdf();
				                      s_pdf_layer.setIndex(embedded_pdf_index[layer->pdf()]);
				                      s_pdf_layer.setPage(layer->page_number());
			                      }},
			           layer);
		}
	}
	qDebug() << "Construct capnp" << construct_timer.elapsed();
	kj::VectorOutputStream out;
	QElapsedTimer write_packed_timer;
	write_packed_timer.start();
	capnp::writePackedMessage(out, message);
	qDebug() << "writePackedMessage" << write_packed_timer.elapsed();
	QElapsedTimer write_bytes_timer;
	write_bytes_timer.start();
	stream.writeBytes(out.getArray().asChars().begin(), out.getArray().size());
	qDebug() << "writeBytes" << write_bytes_timer.elapsed();
}

void load_path_4(file4::Path::Reader s_path, PathStroke* path) {
	auto s_points = s_path.getPoints();
	if (s_points.size() == 0)
		throw SauklaueReadException(QCoreApplication::tr("Invalid Sauklaue file: Empty path."));
	path->reserve_points(s_points.size());
	for (auto s_point : s_points) {
		Point point(s_point.getX(), s_point.getY());
		path->push_back(point);
	}
}

std::unique_ptr<Document> Serializer::load(QDataStream& stream) {
	char magic_string_in[magic_string.size() + 10];
	if (stream.readRawData(magic_string_in, magic_string.size()) != magic_string.size())
		throw SauklaueReadException(QCoreApplication::tr("Not a Sauklaue file."));
	if (std::string_view(magic_string_in, magic_string.size()) != magic_string)
		throw SauklaueReadException(QCoreApplication::tr("Not a Sauklaue file."));
	uint32_t file_format_version;
	stream >> file_format_version;
	if (stream.status() != QDataStream::Ok)
		throw SauklaueReadException(QCoreApplication::tr("Not a Sauklaue file."));
	qDebug() << "File format version" << file_format_version;
	if (file_format_version > FILE_FORMAT_VERSION)
		throw SauklaueReadException(QCoreApplication::tr("File format %1 too new. Only formats up to %2 supported. Consider updating.").arg(file_format_version).arg(FILE_FORMAT_VERSION));
	if (file_format_version < 4)
		throw SauklaueReadException(QCoreApplication::tr("Not a Sauklaue file."));
	stream.setVersion(QDataStream::Qt_5_15);
	char* c_data;
	uint len;
	stream.readBytes(c_data, len);
	std::string data(c_data, len);
	delete[] c_data;
	if (stream.status() != QDataStream::Ok)
		throw SauklaueReadException(QCoreApplication::tr("Invalid Sauklaue file: Too short."));
	auto doc = std::make_unique<Document>();
	if (file_format_version >= 4) {
		kj::ArrayInputStream in(kj::arrayPtr((unsigned char*)data.c_str(), len));
		capnp::ReaderOptions opt;
		// Set the traversalLimitInWords to 512MiB. This means that we in particular can't read any files larger than 512MiB.
		// TODO Figure out a better traversalLimitInWords.
		opt.traversalLimitInWords = 64 * 1024 * 1024;
		capnp::PackedMessageReader message(in, opt);
		auto s_file = message.getRoot<file4::File>();
		std::vector<EmbeddedPDF*> pdfs;
		try {
			for (auto s_pdf : s_file.getEmbeddedPDFs()) {
				auto s_contents = s_pdf.getContents();
				auto pdf = std::make_unique<EmbeddedPDF>(QString::fromStdString(s_pdf.getName()), QByteArray((const char*)s_contents.begin(), s_contents.size()));
				pdfs.push_back(pdf.get());
				doc->add_embedded_pdf(std::move(pdf));
			}
		} catch (const PDFReadException& e) {
			throw SauklaueReadException(QCoreApplication::tr("Invalid embedded pdf file: %1").arg(e.reason()));
		}
		for (auto s_page : s_file.getPages()) {
			auto page = std::make_unique<SPage>(s_page.getWidth(), s_page.getHeight());
			for (auto s_layer : s_page.getLayers()) {
				switch (s_layer.which()) {
				case file4::Layer::NORMAL: {
					auto s_normal_layer = s_layer.getNormal();
					auto layer = std::make_unique<NormalLayer>();
					auto s_strokes = s_normal_layer.getStrokes();
					layer->reserve_strokes(s_strokes.size());
					for (auto s_stroke : s_strokes) {
						unique_ptr_Stroke stroke;
						switch (s_stroke.which()) {
						case file4::Stroke::PEN: {
							auto s_special_stroke = s_stroke.getPen();
							auto special_stroke = std::make_unique<PenStroke>(s_special_stroke.getWidth(), s_special_stroke.getColor());
							load_path_4(s_special_stroke.getPath(), special_stroke.get());
							stroke = std::move(special_stroke);
							break;
						}
						case file4::Stroke::ERASER: {
							auto s_special_stroke = s_stroke.getEraser();
							auto special_stroke = std::make_unique<EraserStroke>(s_special_stroke.getWidth());
							load_path_4(s_special_stroke.getPath(), special_stroke.get());
							stroke = std::move(special_stroke);
							break;
						}
						default:
							throw SauklaueReadException(QCoreApplication::tr("Invalid Sauklaue file: Unknown stroke type."));
						}
						layer->add_stroke(std::move(stroke));
					}
					page->add_layer(page->layers().size(), std::move(layer));
					break;
				}
				case file4::Layer::PDF: {
					auto s_pdf_layer = s_layer.getPdf();
					auto layer = std::make_unique<PDFLayer>(pdfs.at(s_pdf_layer.getIndex()), s_pdf_layer.getPage());
					page->add_layer(page->layers().size(), std::move(layer));
					break;
				}
				default:
					throw SauklaueReadException(QCoreApplication::tr("Invalid Sauklaue file: Unknown layer type."));
				}
			}
			doc->add_page(doc->pages().size(), std::move(page));
		}
	}
	qDebug() << "Read file";
	qDebug() << "Number of pages:" << doc->pages().size();
	int num_strokes = 0, num_points = 0;
	for (auto page : doc->pages()) {
		for (auto layer : page->layers()) {
			std::visit(overloaded{[&](NormalLayer* layer) {
				                      for (auto stroke : layer->strokes()) {
					                      num_strokes++;
					                      std::visit(overloaded{[&](PathStroke* st) {
						                                 num_points += st->points().size();
					                                 }},
					                                 stroke);
				                      }
			                      },
			                      [&](PDFLayer*) {
			                      }},
			           layer);
		}
	}
	qDebug() << "Number of strokes:" << num_strokes;
	qDebug() << "Number of points:" << num_points;
	return doc;
}
