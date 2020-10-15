#include "settings.h"

#include "tablet.h"

Settings* settings_singleton = nullptr;

Settings* Settings::self() {
	if (!settings_singleton)
		settings_singleton = new Settings;
	return settings_singleton;
}

Settings::Settings() :
    ConfigGenerated() {
}

std::vector<TabletSettings> Settings::tablets() const {
	return m_tabletsvec;
}

void Settings::set_tablets(const std::vector<TabletSettings>& tablets) {
	m_tablets.clear();
	m_tabletsvec.clear();
	for (const TabletSettings& tablet : tablets) {
		if (!m_tablets.count(tablet.name)) {
			m_tablets.emplace(tablet.name, tablet);
			m_tabletsvec.push_back(tablet);
		}
	}
}

std::optional<TabletSettings> Settings::tablet(const QString name) const {
	if (m_tablets.count(name))
		return m_tablets.find(name)->second;
	else
		return std::nullopt;
}

void Settings::usrRead() {
	ConfigGenerated::usrRead();
	m_tablets.clear();
	KConfigGroup grp = config()->group("Tablets");
	for (QString name : grp.groupList()) {
		if (m_tablets.count(name))
			continue;
		KConfigGroup tgrp = grp.group(name);
		TabletSettings tablet(name);
		tablet.enabled = tgrp.readEntry("Enabled", true);
		tablet.orientation = tgrp.readEntry("Orientation", 0);
		tablet.width = tgrp.readEntry("Width", 0);
		tablet.height = tgrp.readEntry("Height", 0);
		m_tablets.emplace(name, tablet);
		m_tabletsvec.push_back(tablet);
	}
}

bool Settings::usrSave() {
	if (!ConfigGenerated::usrSave())
		return false;
	KConfigGroup grp = config()->group("Tablets");
	for (const TabletSettings& tablet : m_tabletsvec) {
		if (tablet.isDefault()) {
			grp.deleteGroup(tablet.name);
		} else {
			KConfigGroup tgrp = grp.group(tablet.name);
			tgrp.writeEntry("Enabled", tablet.enabled);
			tgrp.writeEntry("Orientation", tablet.orientation);
			tgrp.writeEntry("Width", tablet.width);
			tgrp.writeEntry("Height", tablet.height);
		}
	}
	return true;
}
