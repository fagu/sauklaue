#ifndef CAIRO_HELPERS_H
#define CAIRO_HELPERS_H

#include <cairomm/matrix.h>

#include <QRectF>

inline void bounding_rect_helper(Cairo::Matrix mat, double x, double y, double& minx, double& maxx, double& miny, double& maxy) {
	mat.transform_point(x, y);
	if (minx > x)
		minx = x;
	if (maxx < x)
		maxx = x;
	if (miny > y)
		miny = y;
	if (maxy < y)
		maxy = y;
}

// Apply the matrix to the rectangle. Then return the (axis-parallel) bounding rectangle of the result.
inline QRectF bounding_rect(Cairo::Matrix mat, QRectF rect) {
	double minx = std::numeric_limits<double>::infinity(), maxx = -std::numeric_limits<double>::infinity(), miny = std::numeric_limits<double>::infinity(), maxy = -std::numeric_limits<double>::infinity();
	bounding_rect_helper(mat, rect.left(), rect.top(), minx, maxx, miny, maxy);
	bounding_rect_helper(mat, rect.left(), rect.bottom(), minx, maxx, miny, maxy);
	bounding_rect_helper(mat, rect.right(), rect.top(), minx, maxx, miny, maxy);
	bounding_rect_helper(mat, rect.right(), rect.bottom(), minx, maxx, miny, maxy);
	return QRectF(QPointF(minx, miny), QPointF(maxx, maxy));
}

#endif
