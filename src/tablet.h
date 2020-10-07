#ifndef TABLET_H
#define TABLET_H

#include <QObject>
#include <QTimer>

#include <cairomm/matrix.h>

struct _XDisplay;

class Tablet : public QObject
{
	Q_OBJECT
public:
	Tablet();
	~Tablet();
	// Ask to set the tablet coordinate transformation matrix. This will be done at the earliest convenience using a QTimer with timeout 10ms.
	void set_transformation_matrix(Cairo::Matrix mat);
private slots:
	void time_to_set_transformation_matrix();
private:
	Cairo::Matrix transformation_matrix;
	QTimer* transformation_matrix_timer; // Quick on-demand one-shot timer
	QTimer* transformation_matrix_timer_backup; // Occasionally try to set the transformation matrix just in case a tablet/pen was connected in the meantime.
	_XDisplay* display;
};

#endif // TABLET_H
