#include "document.h"

#include "util.h"

#include <cassert>
#include <iostream>

#include <QDebug>

const Color Color::BLACK = Color::color(0,0,0,255);



NormalLayer::NormalLayer(const NormalLayer& a)
{
	// Copy the strokes.
	reserve_strokes(a.m_strokes.size());
	for (const unique_ptr_Stroke &s : a.m_strokes) {
		m_strokes.emplace_back(std::visit(
			[](auto* t) -> unique_ptr_Stroke {
				return std::make_unique<typename std::remove_pointer_t<decltype(t)> >(*t);
			}, get(s)));
	}
}



Page::Page(const Page& a) : Page(a.m_width, a.m_height)
{
	// Copy the layers.
	for (const std::unique_ptr<NormalLayer> &l : a.m_layers) {
		m_layers.emplace_back(std::make_unique<NormalLayer>(*l));
	}
}



Document::Document(const Document& a)
{
	// Copy the pages.
	for (const std::unique_ptr<Page> &p : a.m_pages) {
		m_pages.emplace_back(std::make_unique<Page>(*p));
	}
}


std::unique_ptr<Document> Document::concatenate(const std::vector<std::unique_ptr<Document> >& in_docs)
{
	std::unique_ptr<Document> res = std::make_unique<Document>();
	// Copy the pages.
	for (const std::unique_ptr<Document> &a : in_docs) {
		for (const std::unique_ptr<Page> &p : a->m_pages) {
			res->m_pages.emplace_back(std::make_unique<Page>(*p));
		}
	}
	return res;
}
