#ifndef TOOLSTATE_H
#define TOOLSTATE_H

#include <QObject>
#include <QColor>

class QUndoStack;

class ToolState : public QObject {
	Q_OBJECT
public:
	ToolState(QObject* parent = nullptr);

public:
	QUndoStack* undoStack() {
		return m_undoStack;
	}

private:
	QUndoStack* m_undoStack;

public:
	QColor penColor() {
		return m_pen_color;
	}
	void setPenColor(QColor pen_color);

private:
	QColor m_pen_color;

public:
	int penSize() {
		return m_pen_size;
	}
	void setPenSize(int pen_size);

private:
	int m_pen_size;
};

#endif  // TOOLSTATE_H
