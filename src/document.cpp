#include "document.h"

#include "util.h"

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
	reserve_strokes(a.m_strokes.size());
	for (const unique_ptr_Stroke &s : a.m_strokes) {
		m_strokes.emplace_back(std::visit(
			[](auto* t) -> unique_ptr_Stroke {
				return std::make_unique<typename std::remove_pointer_t<decltype(t)> >(*t);
			}, get(s)));
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
