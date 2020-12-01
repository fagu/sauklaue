#include "document.h"

#include <cassert>
#include <iostream>

#include <QTimer>

// The header poppler.h defines a variable called signals, which is a qt keyword.
#undef signals
#include <poppler.h>
#define signals Q_SIGNALS

const Color Color::BLACK = Color::color(0, 0, 0, 255);

FadingStroke::FadingStroke(TemporaryLayer* layer, unique_ptr_Stroke stroke) :
    m_layer(layer),
    m_stroke(std::move(stroke)) {
}

void FadingStroke::start(std::list<FadingStroke>::iterator it, int timeout) {
	m_it = it;
	m_timer = new QTimer(this);
	m_timer->setSingleShot(true);
	connect(m_timer, &QTimer::timeout, this, &FadingStroke::timeout);
	m_timer->start(timeout);
}

void FadingStroke::timeout() {
	// Delete the stroke, handing back ownership over the stroke object to the layer.
	// This also deletes the calling TemporaryStroke object, which is fine!
	m_layer->delete_stroke(m_it, std::move(m_stroke));
}

NormalLayer::NormalLayer(const NormalLayer& a) {
	// Copy the strokes.
	reserve_strokes(a.strokes().size());
	for (ptr_Stroke s : a.strokes()) {
		m_strokes.emplace_back(std::visit(
		        [](auto* t) -> unique_ptr_Stroke {
			        return std::make_unique<typename std::remove_pointer_t<decltype(t)> >(*t);
		        },
		        s));
	}
}

template <class T>
GObjectWrapper<T>::~GObjectWrapper<T>() {
	if (m_value)
		g_object_unref(m_value);
}

template <class T>
void GObjectWrapper<T>::reset(T* value) {
	if (m_value == value)
		return;
	if (m_value)
		g_object_unref(m_value);
	m_value = value;
}

template class GObjectWrapper<_PopplerDocument>;
template class GObjectWrapper<_PopplerPage>;

std::pair<int, int> PDFLayer::size() const {
	double width, height;
	poppler_page_get_size(page(), &width, &height);
	return {width * POINT_TO_UNIT, height * POINT_TO_UNIT};
}

// Page::Page(const Page& a) : Page(a.m_width, a.m_height)
// {
// 	// Copy the layers.
// 	for (NormalLayer* l : a.layers())
// 		m_layers.emplace_back(std::make_unique<NormalLayer>(*l));
// }

EmbeddedPDF::EmbeddedPDF(const QString& name, const QByteArray& contents) :
    m_name(name), m_contents(contents) {
	GBytes* data = g_bytes_new(m_contents.data(), m_contents.length());
	GError* err = nullptr;
	m_document.reset(poppler_document_new_from_bytes(data, nullptr, &err));
	g_bytes_unref(data);
	if (err)
		throw PDFReadException("Invalid pdf file.");
	if (!m_document)
		throw PDFReadException("Invalid pdf file.");
	for (int page_number = 0; page_number < poppler_document_get_n_pages(document()); page_number++) {
		m_pages.emplace_back(poppler_document_get_page(document(), page_number));
		if (!m_pages[page_number])
			throw PDFReadException("Invalid page " + QString::number(page_number + 1));
	}
}

std::vector<std::pair<int, int> > EmbeddedPDF::page_label_ranges() const {
	std::vector<std::pair<int, int> > res;
	char* prev_label = nullptr;
	int start = 0;
	for (int i = 0; i < (int)pages().size(); i++) {
		char* cur_label = poppler_page_get_label(pages()[i]);
		if (prev_label && strcmp(prev_label, cur_label)) {
			res.emplace_back(start, i - 1);
			start = i;
		}
		prev_label = cur_label;
	}
	if (prev_label) {
		res.emplace_back(start, (int)pages().size() - 1);
	}
	return res;
}

// Document::Document(const Document& a) : QObject()
// {
// 	// Copy the pages.
// 	for (Page* p : a.pages())
// 		m_pages.emplace_back(std::make_unique<Page>(*p));
// }

// std::unique_ptr<Document> Document::concatenate(const std::vector<std::unique_ptr<Document> >& in_docs)
// {
// 	std::unique_ptr<Document> res = std::make_unique<Document>();
// 	// Copy the pages.
// 	for (const std::unique_ptr<Document> &a : in_docs) {
// 		for (Page* p : a->pages()) {
// 			res->m_pages.emplace_back(std::make_unique<Page>(*p));
// 		}
// 	}
// 	return res;
// }
