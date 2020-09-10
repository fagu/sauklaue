#include "document.h"

#include "util.h"

#include <cassert>
#include <iostream>

#include <QDebug>

const Color Color::BLACK = Color::color(0,0,0,255);


TemporaryStroke::TemporaryStroke(NormalLayer* layer, unique_ptr_Stroke stroke) :
	m_layer(layer),
	m_stroke(std::move(stroke))
{
}

void TemporaryStroke::start(std::list<TemporaryStroke>::iterator it, int timeout)
{
	m_it = it;
	m_timer = new QTimer(this);
	m_timer->setSingleShot(true);
	connect(m_timer, &QTimer::timeout, this, &TemporaryStroke::timeout);
	m_timer->start(timeout);
}

void TemporaryStroke::timeout()
{
	// Delete the stroke, handing back ownership over the stroke object to the layer.
	// This also deletes the calling TemporaryStroke object, which is fine!
	m_layer->delete_temporary_stroke(m_it, std::move(m_stroke));
}



NormalLayer::NormalLayer(const NormalLayer& a)
{
	// Copy the strokes.
	reserve_strokes(a.strokes().size());
	for (ptr_Stroke s : a.strokes()) {
		m_strokes.emplace_back(std::visit(
			[](auto* t) -> unique_ptr_Stroke {
				return std::make_unique<typename std::remove_pointer_t<decltype(t)> >(*t);
			}, s));
	}
}



Page::Page(const Page& a) : Page(a.m_width, a.m_height)
{
	// Copy the layers.
	for (NormalLayer* l : a.layers())
		m_layers.emplace_back(std::make_unique<NormalLayer>(*l));
}



Document::Document(const Document& a)
{
	// Copy the pages.
	for (Page* p : a.pages())
		m_pages.emplace_back(std::make_unique<Page>(*p));
}


std::unique_ptr<Document> Document::concatenate(const std::vector<std::unique_ptr<Document> >& in_docs)
{
	std::unique_ptr<Document> res = std::make_unique<Document>();
	// Copy the pages.
	for (const std::unique_ptr<Document> &a : in_docs) {
		for (Page* p : a->pages()) {
			res->m_pages.emplace_back(std::make_unique<Page>(*p));
		}
	}
	return res;
}
