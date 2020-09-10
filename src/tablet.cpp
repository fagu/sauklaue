#include "tablet.h"

#include <QDebug>
#include <QProcess>

Tablet::Tablet()
{
	transformation_matrix_timer = new QTimer(this);
	transformation_matrix_timer->setSingleShot(true);
	connect(transformation_matrix_timer, &QTimer::timeout, this, &Tablet::time_to_set_transformation_matrix);
}

Tablet::~Tablet()
{
	// Immediately reset the transformation matrix to the identity matrix.
	transformation_matrix = Cairo::identity_matrix();
	time_to_set_transformation_matrix();
}

void Tablet::set_transformation_matrix(Cairo::Matrix mat)
{
	transformation_matrix = mat;
	transformation_matrix_timer->start(10);
}

void Tablet::time_to_set_transformation_matrix()
{
	qDebug() << "Setting transformation matrix.";
	double dx = 0, dy = 0;
	transformation_matrix.transform_point(dx, dy);
	double e1x = 1, e1y = 0;
	transformation_matrix.transform_distance(e1x, e1y);
	double e2x = 0, e2y = 1;
	transformation_matrix.transform_distance(e2x, e2y);
	QProcess::execute("xinput", {"set-prop", "XPPEN Tablet Pen (0)", "--type=float", "Coordinate Transformation Matrix", QString::number(e1x), QString::number(e2x), QString::number(dx), QString::number(e1y), QString::number(e2y), QString::number(dy), "0", "0", "1"});
}
