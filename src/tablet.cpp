#include "tablet.h"

#include <QDebug>
#include <QProcess>
#include <X11/extensions/XInput2.h>
// #include <QtX11Extras/QX11Info>

Tablet::Tablet()
{
	transformation_matrix = Cairo::identity_matrix();
	transformation_matrix_timer = new QTimer(this);
	transformation_matrix_timer->setSingleShot(true);
	transformation_matrix_timer_backup = new QTimer(this);
	connect(transformation_matrix_timer, &QTimer::timeout, this, &Tablet::time_to_set_transformation_matrix);
	connect(transformation_matrix_timer_backup, &QTimer::timeout, this, &Tablet::time_to_set_transformation_matrix);
// 	display = QX11Info::display();
	display = XOpenDisplay(nullptr);
	if (!display) {
		qDebug() << "No X11 display!";
		return;
	}
	int xi_opcode, event, error;
    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
		qDebug() << "X Input extension not available.";
		display = nullptr;
		return;
	}
    // Print a list of all devices.
    // See list_xi2() in https://cgit.freedesktop.org/xorg/app/xinput/tree/src/list.c
	int ndevices;
	XIDeviceInfo *info;
	info = XIQueryDevice(display, XIAllDevices, &ndevices);
	for (int i = 0; i < ndevices; i++) {
		XIDeviceInfo* device = &info[i];
		qDebug() << "Device" << device->deviceid << device->name;
	}
	XIFreeDeviceInfo(info);
	// TODO Instead of trying to set the transformation matrix once every second, watch out for "device connected" signals.
	transformation_matrix_timer_backup->start(1000);
}

Tablet::~Tablet()
{
	// Immediately reset the transformation matrix to the identity matrix.
	transformation_matrix = Cairo::identity_matrix();
	time_to_set_transformation_matrix();
	XCloseDisplay(display);
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
// 	QProcess::execute("xinput", {"set-prop", "XPPEN Tablet Pen (0)", "--type=float", "Coordinate Transformation Matrix", QString::number(e1x), QString::number(e2x), QString::number(dx), QString::number(e1y), QString::number(e2y), QString::number(dy), "0", "0", "1"});
	float mat[9] = {(float)e1x, (float)e2x, (float)dx, (float)e1y, (float)e2y, (float)dy, 0, 0, 1};
	if (!display) {
		qDebug() << "No X11 display!";
		return;
	}
	// See do_set_prop_xi2() in https://cgit.freedesktop.org/xorg/app/xinput/tree/src/property.c
	int ndevices;
	XIDeviceInfo *info;
	info = XIQueryDevice(display, XIAllDevices, &ndevices);
	bool found = false;
	for (int i = 0; i < ndevices; i++) {
		XIDeviceInfo* device = &info[i];
		if (std::string(device->name) == "XPPEN Tablet Pen (0)") {
			static_assert(sizeof(float) == 4);
			XIChangeProperty(display, device->deviceid, XInternAtom(display, "Coordinate Transformation Matrix", False), XInternAtom(display, "FLOAT", False), 32, PropModeReplace, (unsigned char*)mat, 9);
			found = true;
		}
	}
	if (!found)
		qDebug() << "Couldn't find device.";
	XIFreeDeviceInfo(info);
	// I don't understand why, but somehow the changed coordinate transformation matrix is not always applied immediately.
	// However, it seems to be applied the next time we get a list of all devices, or when we close the display.
	info = XIQueryDevice(display, XIAllDevices, &ndevices);
	XIFreeDeviceInfo(info);
}
