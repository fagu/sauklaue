#ifndef SETTINGS_H
#define SETTINGS_H

#include "config.h"

#include <QString>
#include <QDialog>

#include <map>
#include <optional>

class TabletSettings {
public:
	QString name;  // Name of the input device
	bool enabled;  // Whether the input device coordinates should be transformed
	int orientation;  // Real rotation of the input device in degrees (clockwise)
	int width, height;  // Real width and height of the input device (before rotation)
	TabletSettings(QString _name) :
	    name(_name), enabled(false), orientation(0), width(100), height(100) {
	}
	TabletSettings(const TabletSettings&) = default;
};

class Settings : public ConfigGenerated {
	Q_OBJECT
public:
	static Settings* self();
	std::vector<TabletSettings> tablets() const;
	void set_tablets(const std::vector<TabletSettings>& tablets);
	std::optional<TabletSettings> tablet(const QString name) const;

private:
	Settings();

protected:
	virtual void usrRead();
	virtual bool usrSave();

private:
	std::map<QString, TabletSettings> m_tablets;
	std::vector<TabletSettings> m_tabletsvec;
};

#endif
