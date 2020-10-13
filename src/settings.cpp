#include "settings.h"

#include "tablet.h"

#include <QtWidgets>

#include <set>



Settings * settings_singleton = nullptr;

Settings * Settings::self()
{
	if (!settings_singleton)
		settings_singleton = new Settings;
	return settings_singleton;
}


Settings::Settings() : ConfigGenerated() {
}

std::vector<TabletSettings> Settings::tablets() const {
	return m_tabletsvec;
}

void Settings::set_tablets(const std::vector<TabletSettings> &tablets) {
	m_tablets.clear();
	m_tabletsvec.clear();
	for (const TabletSettings &tablet : tablets) {
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

void Settings::usrRead()
{
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
		KConfigGroup tgrp = grp.group(tablet.name);
		tgrp.writeEntry("Enabled", tablet.enabled);
		tgrp.writeEntry("Orientation", tablet.orientation);
		tgrp.writeEntry("Width", tablet.width);
		tgrp.writeEntry("Height", tablet.height);
	}
	return true;
}

TabletRow::TabletRow(const TabletSettings& tablet, QGridLayout* tabletGrid, int i, bool connected)
{
	m_name = tablet.name;
	m_orientation_degrees = tablet.orientation;
	m_enabled = new QCheckBox;
	m_enabled->setChecked(tablet.enabled);
	QIcon icon = QIcon::fromTheme(connected ? "network-connect" : "network-disconnect");
	m_enabled->setIconSize(QSize(32,32));
	m_enabled->setIcon(icon);
	m_enabled->setText(tablet.name);
	connect(m_enabled, &QCheckBox::toggled, this, &TabletRow::update);
	tabletGrid->addWidget(m_enabled, i+1, 0);
	m_rotate_left = new QPushButton(QIcon::fromTheme("object-rotate-left"), "");
	connect(m_rotate_left, &QPushButton::clicked, this, &TabletRow::rotateLeft);
	tabletGrid->addWidget(m_rotate_left, i+1, 1);
	m_orientation = new QLabel;
	m_orientation->setMinimumWidth(45);
	m_orientation->setAlignment(Qt::AlignRight);
	tabletGrid->addWidget(m_orientation, i+1, 2);
	m_rotate_right = new QPushButton(QIcon::fromTheme("object-rotate-right"), "");
	connect(m_rotate_right, &QPushButton::clicked, this, &TabletRow::rotateRight);
	tabletGrid->addWidget(m_rotate_right, i+1, 3);
	m_width = new QDoubleSpinBox;
	m_width->setDecimals(1);
	m_width->setMinimum(0.1);
	m_width->setValue(0.1*tablet.width);
	tabletGrid->addWidget(m_width, i+1, 4);
	m_height = new QDoubleSpinBox;
	m_height->setDecimals(1);
	m_height->setMinimum(0.1);
	m_height->setValue(0.1*tablet.height);
	tabletGrid->addWidget(m_height, i+1, 5);
	update();
}

void TabletRow::update()
{
	m_orientation->setText(QString::number(m_orientation_degrees) + "°");
	m_orientation->setEnabled(m_enabled->isChecked());
	m_rotate_left->setEnabled(m_enabled->isChecked());
	m_rotate_right->setEnabled(m_enabled->isChecked());
	m_width->setEnabled(m_enabled->isChecked());
	m_height->setEnabled(m_enabled->isChecked());
}

void TabletRow::rotateLeft()
{
	m_orientation_degrees = (m_orientation_degrees + 270)%360;
	update();
}

void TabletRow::rotateRight()
{
	m_orientation_degrees = (m_orientation_degrees + 90)%360;
	update();
}

TabletSettings TabletRow::get() const
{
	TabletSettings res(m_name);
	res.enabled = m_enabled->isChecked();
	res.orientation = m_orientation_degrees;
	res.width = qRound(m_width->value()*10);
	res.height = qRound(m_height->value()*10);
	return res;
}




SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);
	
	{
		QLabel *label = new QLabel(tr("Check the external tablet/pen device(s) whose writing area should be mapped only onto the current page. Also input the orientation of the tablet and the correct width/height ratio (before rotation)."));
		label->setWordWrap(true);
		layout->addWidget(label);
	}
	
	tabletGrid = new QGridLayout;
	layout->addLayout(tabletGrid);
	
	{
		QLabel *label = new QLabel(tr("Device"));
		QFont font = label->font();
		font.setBold(true);
		label->setFont(font);
		tabletGrid->addWidget(label, 0, 0);
	}
	{
		QLabel *label = new QLabel(tr("Rotation"));
		label->setAlignment(Qt::AlignCenter);
		QFont font = label->font();
		font.setBold(true);
		label->setFont(font);
		tabletGrid->addWidget(label, 0, 1, 1, 3);
	}
	{
		QLabel *label = new QLabel(tr("Width"));
		label->setAlignment(Qt::AlignCenter);
		QFont font = label->font();
		font.setBold(true);
		label->setFont(font);
		tabletGrid->addWidget(label, 0, 4);
	}
	{
		QLabel *label = new QLabel(tr("Height"));
		label->setAlignment(Qt::AlignCenter);
		QFont font = label->font();
		font.setBold(true);
		label->setFont(font);
		tabletGrid->addWidget(label, 0, 5);
	}
	
	QHBoxLayout *bottom = new QHBoxLayout;
	QPushButton *reloadButton = new QPushButton(QIcon::fromTheme("view-refresh"), tr("Reload"));
	connect(reloadButton, &QPushButton::clicked, this, &SettingsDialog::reload);
	bottom->addWidget(reloadButton);
	bottom->addStretch();
	layout->addLayout(bottom);
	QPushButton *okButton = new QPushButton(QIcon::fromTheme("dialog-ok-apply"), tr("OK"));
	connect(okButton, &QPushButton::clicked, this, &SettingsDialog::ok);
	bottom->addWidget(okButton);
	QPushButton *cancelButton = new QPushButton(QIcon::fromTheme("dialog-cancel"), tr("Cancel"));
	connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::cancel);
	bottom->addWidget(cancelButton);
	
	reload();
	
	setModal(true);
	
	setWindowTitle(tr("Settings"));
}

void SettingsDialog::reload()
{
	while (QLayoutItem *child = tabletGrid->takeAt(6)) {
		delete child->widget(); // delete the widget
		delete child;   // delete the layout item
	}
	std::set<QString> tablet_names;
	std::vector<TabletSettings> tablet_settings = Settings::self()->tablets();
	for (const TabletSettings &tablet : tablet_settings)
		tablet_names.insert(tablet.name);
	m_rows.clear();
	std::set<QString> connected_tablets;
	for (const QString &tablet_name : TabletHandler::self()->device_list()) {
		connected_tablets.insert(tablet_name);
		if (!tablet_names.count(tablet_name)) {
			tablet_settings.emplace_back(tablet_name);
			tablet_names.insert(tablet_name);
		}
	}
	std::sort(tablet_settings.begin(), tablet_settings.end(), [](const TabletSettings &a, const TabletSettings &b) {return a.name < b.name;});
	for (int i = 0; i < (int)tablet_settings.size(); i++) {
		m_rows.push_back(std::make_unique<TabletRow>(tablet_settings[i], tabletGrid, i+1, connected_tablets.count(tablet_settings[i].name)));
	}
}

void SettingsDialog::ok()
{
	std::vector<TabletSettings> tablets;
	for (const auto& row : m_rows)
		tablets.push_back(row->get());
	Settings::self()->set_tablets(tablets);
	Settings::self()->save();
	close();
}

void SettingsDialog::cancel()
{
	close();
}
