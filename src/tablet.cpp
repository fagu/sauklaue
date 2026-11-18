#include "tablet.h"

#include "settings.h"

#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QDBusConnectionInterface>
#include <X11/extensions/XInput2.h>
// #include <QtX11Extras/QX11Info>

class InterfaceX11 : public TabletInterface {
private:
	_XDisplay* display;
public:
	InterfaceX11(_XDisplay* display_) : display(display_) {}
	~InterfaceX11() {
		XCloseDisplay(display);
	}
	std::vector<std::pair<int,QString>> device_list() {
		std::vector<std::pair<int,QString>> tablets;
		// Print a list of all devices.
		// See list_xi2() in https://cgit.freedesktop.org/xorg/app/xinput/tree/src/list.c
		int ndevices;
		XIDeviceInfo* info = XIQueryDevice(display, XIAllDevices, &ndevices);
		for (int i = 0; i < ndevices; i++) {
			XIDeviceInfo* device = &info[i];
			qDebug() << "Device" << device->deviceid << device->name << device->attachment << device->use;
			if (device->use == XIMasterPointer || device->use == XISlavePointer)
				tablets.emplace_back(device->deviceid, device->name);
		}
		XIFreeDeviceInfo(info);
		return tablets;
	}
	void set_matrix(int device_id, QMatrix cmat) {
		float mat[9] = {(float)cmat.m11(), (float)cmat.m21(), (float)cmat.dx(), (float)cmat.m12(), (float)cmat.m22(), (float)cmat.dy(), 0, 0, 1};
		XIChangeProperty(display, device_id, XInternAtom(display, "Coordinate Transformation Matrix", False), XInternAtom(display, "FLOAT", False), 32, PropModeReplace, (unsigned char*)mat, 9);
	}
	void commit() {
		// I don't understand why, but somehow the changed coordinate transformation matrix is not always applied immediately.
		// However, it seems to be applied the next time we get a list of all devices, or when we close the display.
		int ndevices;
		XIDeviceInfo* info = XIQueryDevice(display, XIAllDevices, &ndevices);
		XIFreeDeviceInfo(info);
	}
};

InterfaceX11* make_x11() {
	// 	display = QX11Info::display();
	_XDisplay* display = XOpenDisplay(nullptr);
	if (!display)
		return nullptr;
	int xi_opcode, event, error;
	if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
		qDebug() << "X Input extension not available.";
		XCloseDisplay(display);
		return nullptr;
	}
	
	qDebug() << "Detected X11";
	return new InterfaceX11(display);
}

class InterfaceKWin : public TabletInterface {
public:
	InterfaceKWin(QDBusConnection bus) : m_bus(bus) {}
	~InterfaceKWin() {}
	std::vector<std::pair<int,QString>> device_list() {
		QStringList v = getProperty("/org/kde/KWin/InputDevice", "org.kde.KWin.InputDeviceManager", "devicesSysNames").toStringList();
		std::vector<std::pair<int,QString>> tablets;
		for (const QString& sys : v) {
			if (!sys.startsWith("event")) {
				qDebug() << "Invalid sys string" << sys;
				continue;
			}
			bool ok;
			int device_id = sys.mid(5).toInt(&ok);
			if (!ok) {
				qDebug() << "Invalid sys string" << sys;
				continue;
			}
            const QString path = "/org/kde/KWin/InputDevice/" + sys;
            if (!getProperty(path, "org.kde.KWin.InputDevice", "tabletTool").toBool())
                continue;
            tablets.emplace_back(device_id, getProperty(path, "org.kde.KWin.InputDevice", "name").toString());
		}
		return tablets;
	}
	void set_matrix(int device_id, QMatrix cmat) {
		const double mat[16] = {cmat.m11(), cmat.m21(), cmat.dx(), 0, cmat.m12(), cmat.m22(), cmat.dy(), 0, 0, 0, 1, 0, 0, 0, 0, 1};
		QStringList parts;
		for (double x : mat)
			parts << QString::number(x);
		QString smat = parts.join(QLatin1Char(','));
		setProperty("/org/kde/KWin/InputDevice/event" + QString::number(device_id), "org.kde.KWin.InputDevice", "calibrationMatrix", smat);
	}
private:
	QDBusConnection m_bus;
	QVariant getProperty(const QString &path, const QString &iface, const QString &prop) {
		QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.KWin", path, "org.freedesktop.DBus.Properties", QStringLiteral("Get"));
		msg << iface << prop;
		const QDBusMessage reply = m_bus.call(msg);
		if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
			return {};
		return reply.arguments().first().value<QDBusVariant>().variant();
	}
	void setProperty(const QString &path, const QString &iface, const QString &prop, const QVariant &value) {
		QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.KWin", path, "org.freedesktop.DBus.Properties", "Set");
		msg << iface << prop << QVariant::fromValue(QDBusVariant(value));
		const QDBusMessage reply = m_bus.call(msg);
		if (reply.type() == QDBusMessage::ErrorMessage)
			qDebug() << "setting" << prop << "failed" << reply.errorMessage() << reply.errorName();
    }
};

InterfaceKWin* make_kwin() {
	const QByteArray sessionType = qgetenv("XDG_SESSION_TYPE").toLower();
	const bool wayland = sessionType == "wayland" || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
	if (!wayland)
		return nullptr;

	// XDG_CURRENT_DESKTOP is a colon-separated list, Plasma sets "KDE".
	const QList<QByteArray> desktops = qgetenv("XDG_CURRENT_DESKTOP").toUpper().split(':');
	if (!desktops.contains("KDE") && qgetenv("KDE_FULL_SESSION") != "true")
		return nullptr;

	QDBusConnection bus = QDBusConnection::sessionBus();
	auto *busIface = bus.interface();
	if (!bus.isConnected() || !busIface || !busIface->isServiceRegistered("org.kde.KWin"))
		return nullptr;
	
	qDebug() << "Detected KWin";
	
	return new InterfaceKWin(bus);
}

TabletHandler::TabletHandler() {
	m_rect_one = m_rect_both = QRectF(0, 0, 1, 1);  // TODO
	m_screen_size = QSize(1, 1);  // TODO
	on_demand_timer = new QTimer(this);
	on_demand_timer->setSingleShot(true);
	periodic_timer = new QTimer(this);
	connect(on_demand_timer, &QTimer::timeout, this, &TabletHandler::update_matrices_now);
	connect(periodic_timer, &QTimer::timeout, this, &TabletHandler::update_matrices_now);
	// TODO Instead of trying to set the transformation matrix once every second, watch out for "device connected" signals.
	periodic_timer->start(1000);
	
	InterfaceKWin* in = make_kwin();
	if (in) {
		interface.reset(in);
	} else {
		InterfaceX11* in = make_x11();
		if (in)
			interface.reset(in);
	}
}

TabletHandler::~TabletHandler() {
	// Immediately reset the transformation matrix to the identity matrix.
	m_active = false;
	update_matrices_now();
}

std::vector<QString> TabletHandler::device_list() {
	if (!interface)
		return {};
	std::vector<QString> tablet_names;
	for (const auto &pa : interface->device_list())
		tablet_names.push_back(pa.second);
	return tablet_names;
}

void TabletHandler::set_active_region(QRectF rect_one, QRectF rect_both, QSize screen_size) {
	if (m_rect_one == rect_one && m_rect_both == rect_both && m_screen_size == screen_size)
		return;
	m_rect_one = rect_one;
	m_rect_both = rect_both;
	m_screen_size = screen_size;
	if (!on_demand_timer->isActive())
		on_demand_timer->start(10);
}

QMatrix TabletHandler::matrix(const TabletSettings& tablet) const {
	// Without loss of generality, the unit in reality is 1 pixel.
	// Tablet -> Reality
	QMatrix t2r = QMatrix().scale(tablet.width, tablet.height) * QMatrix().rotate(tablet.orientation);
	// Reality -> Tablet
	QMatrix r2t = t2r.inverted();
	// Bounding rectangle for the rectangle to be mapped to in tablet space
	QRectF rect = r2t.mapRect(tablet.bothSides ? m_rect_both : m_rect_one);
	// Rescale the tablet so it becomes at least as large as the bounding rectangle.
	double scale = std::max(rect.width(), rect.height());
	// Translate the tablet so it contains the bounding rectangle.
	double dx = (rect.left() + rect.right() - scale) / 2, dy = (rect.top() + rect.bottom() - scale) / 2;
	return QMatrix().scale(scale, scale) * QMatrix().translate(dx, dy) * t2r * QMatrix().scale(1. / m_screen_size.width(), 1. / m_screen_size.height());
}

// TODO We waste quite a bit of time waiting for the server to respond. It would be better to do this in a separate thread. We could then perhaps even increase the update frequency.
void TabletHandler::update_matrices_now() {
	// 	qDebug() << "Setting transformation matrix.";
	if (!interface)
		return;
	// See do_set_prop_xi2() in https://cgit.freedesktop.org/xorg/app/xinput/tree/src/property.c
	static_assert(sizeof(float) == 4);
	bool active = m_active && (qApp && qApp->activeWindow() != nullptr);
	for (const auto &pa : interface->device_list()) {
		int device_id = pa.first;
		QString device_name = pa.second;
		std::optional<TabletSettings> tablet = Settings::self()->tablet(device_name);
		if (active && tablet && tablet->enabled) {
			m_changed.insert(device_id);
			QMatrix cmat = matrix(*tablet);
			interface->set_matrix(device_id, cmat);
		} else if (auto it = m_changed.find(device_id); it != m_changed.end()) {
			m_changed.erase(it);
			interface->set_matrix(device_id, QMatrix());
		}
	}
	interface->commit();
}
