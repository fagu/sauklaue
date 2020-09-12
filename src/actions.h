#ifndef ACTIONS_H
#define ACTIONS_H

#include "document.h"

#include <QUndoCommand>

class NewPageCommand : public QUndoCommand {
public:
	NewPageCommand(Document* _doc, int _index, std::unique_ptr<SPage> _page, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	Document *doc;
	int index;
	std::unique_ptr<SPage> page;
};

class DeletePageCommand : public QUndoCommand {
public:
	DeletePageCommand(Document* _doc, int _index, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	Document *doc;
	int index;
	std::unique_ptr<SPage> page;
};

template <class StrokeType>
class AddStrokeCommand : public QUndoCommand {
public:
	AddStrokeCommand(NormalLayer* _layer, std::unique_ptr<StrokeType> _stroke, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	NormalLayer* layer;
	std::unique_ptr<StrokeType> stroke;
};

class AddPenStrokeCommand : public AddStrokeCommand<PenStroke> {
public:
	AddPenStrokeCommand(NormalLayer* _layer, std::unique_ptr<PenStroke> _stroke, QUndoCommand *parent=nullptr);
};

class AddEraserStrokeCommand : public AddStrokeCommand<EraserStroke> {
public:
	AddEraserStrokeCommand(NormalLayer* _layer, std::unique_ptr<EraserStroke> _stroke, QUndoCommand *parent=nullptr);
};

class AddEmbeddedPDFCommand : public QUndoCommand {
public:
	AddEmbeddedPDFCommand(Document *doc, std::unique_ptr<EmbeddedPDF> pdf, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	Document *m_doc;
	std::unique_ptr<EmbeddedPDF> m_pdf;
	std::list<std::unique_ptr<EmbeddedPDF> >::iterator m_it;
};

#endif // ACTIONS_H
