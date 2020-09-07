#ifndef ACTIONS_H
#define ACTIONS_H

#include "document.h"

#include <QUndoCommand>

class MainWindow;

class NewPageCommand : public QUndoCommand {
public:
	NewPageCommand(MainWindow* _view, int _index, std::unique_ptr<Page> _page, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	MainWindow *view;
	int index;
	std::unique_ptr<Page> page;
};

class DeletePageCommand : public QUndoCommand {
public:
	DeletePageCommand(MainWindow* _view, int _index, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	MainWindow *view;
	int index;
	std::unique_ptr<Page> page;
};

template <class StrokeType>
class AddStrokeCommand : public QUndoCommand {
public:
	AddStrokeCommand(MainWindow* _view, int _page, int _layer, std::unique_ptr<StrokeType> _stroke, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	MainWindow *view;
	int page;
	int layer;
	std::unique_ptr<StrokeType> stroke;
};

class AddPenStrokeCommand : public AddStrokeCommand<PenStroke> {
public:
	AddPenStrokeCommand(MainWindow* _view, int _page, int _layer, std::unique_ptr<PenStroke> _stroke, QUndoCommand *parent=nullptr);
};

class AddEraserStrokeCommand : public AddStrokeCommand<EraserStroke> {
public:
	AddEraserStrokeCommand(MainWindow* _view, int _page, int _layer, std::unique_ptr<EraserStroke> _stroke, QUndoCommand *parent=nullptr);
};

#endif // ACTIONS_H
