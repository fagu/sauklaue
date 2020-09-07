#ifndef ACTIONS_H
#define ACTIONS_H

#include "document.h"

#include <QUndoCommand>

class NewPageCommand : public QUndoCommand {
public:
	NewPageCommand(Document* _doc, int _index, std::unique_ptr<Page> _page, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	Document *doc;
	int index;
	std::unique_ptr<Page> page;
};

class DeletePageCommand : public QUndoCommand {
public:
	DeletePageCommand(Document* _doc, int _index, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	Document *doc;
	int index;
	std::unique_ptr<Page> page;
};

template <class StrokeType>
class AddStrokeCommand : public QUndoCommand {
public:
	AddStrokeCommand(Document* _doc, int _page, int _layer, std::unique_ptr<StrokeType> _stroke, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	Document *doc;
	int page;
	int layer;
	std::unique_ptr<StrokeType> stroke;
};

class AddPenStrokeCommand : public AddStrokeCommand<PenStroke> {
public:
	AddPenStrokeCommand(Document* _doc, int _page, int _layer, std::unique_ptr<PenStroke> _stroke, QUndoCommand *parent=nullptr);
};

class AddEraserStrokeCommand : public AddStrokeCommand<EraserStroke> {
public:
	AddEraserStrokeCommand(Document* _doc, int _page, int _layer, std::unique_ptr<EraserStroke> _stroke, QUndoCommand *parent=nullptr);
};

#endif // ACTIONS_H
