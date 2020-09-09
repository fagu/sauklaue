#ifndef TABLET_H
#define TABLET_H

#include <QObject>
#include <QTimer>

#include <cairomm/matrix.h>

class Tablet : public QObject
{
	Q_OBJECT
public:
	Tablet();
	~Tablet();
	// Ask to set the tablet coordinate transformation matrix. This will be done at the earliest convenience using a QTimer with timeout 0.
	void set_transformation_matrix(Cairo::Matrix mat);
private slots:
	void time_to_set_transformation_matrix();
private:
	Cairo::Matrix transformation_matrix;
	QTimer* transformation_matrix_timer;
};

#endif // TABLET_H
