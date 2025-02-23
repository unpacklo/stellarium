/*
 * Pointer Coordinates plug-in for Stellarium
 *
 * Copyright (C) 2014 Alexander Wolf
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "PointerCoordinates.hpp"
#include "PointerCoordinatesWindow.hpp"
#include "ui_pointerCoordinatesWindow.h"

#include "StelApp.hpp"
#include "StelModule.hpp"
#include "StelModuleMgr.hpp"
#include "StelGui.hpp"
#include "StelTranslator.hpp"

PointerCoordinatesWindow::PointerCoordinatesWindow()
	: StelDialog("PointerCoordinates")
	, coord(Q_NULLPTR)
{
	ui = new Ui_pointerCoordinatesWindowForm();
}

PointerCoordinatesWindow::~PointerCoordinatesWindow()
{
	delete ui;
}

void PointerCoordinatesWindow::retranslate()
{
	if (dialog)
	{
		ui->retranslateUi(dialog);
		setAboutHtml();
		populateCoordinatesPlacesList();
		populateCoordinateSystemsList();
	}
}

void PointerCoordinatesWindow::createDialogContent()
{
	coord = GETSTELMODULE(PointerCoordinates);
	ui->setupUi(dialog);

	connect(&StelApp::getInstance(), SIGNAL(languageChanged()), this, SLOT(retranslate()));
	connect(ui->closeStelWindow, SIGNAL(clicked()), this, SLOT(close()));
	connect(ui->TitleBar, SIGNAL(movedTo(QPoint)), this, SLOT(handleMovedTo(QPoint)));

	connectBoolProperty(ui->checkBoxEnableAtStartup, "PointerCoordinates.enableAtStartup");
	connectBoolProperty(ui->checkBoxShowButton,      "PointerCoordinates.showCoordinatesButton");
	connectBoolProperty(ui->checkBoxConstellation,   "PointerCoordinates.showConstellation");
	connectBoolProperty(ui->checkBoxCrossedLines,    "PointerCoordinates.showCrossedLines");
	connectBoolProperty(ui->checkBoxElongation,      "PointerCoordinates.showElongation");
	connectIntProperty(ui->spinBoxFontSize,          "PointerCoordinates.fontSize");
	connectColorButton(ui->textColorButton, "PointerCoordinates.fontColor", "PointerCoordinates/text_color");

	// Place of the string with coordinates
	populateCoordinatesPlacesList();
	QString currentPlaceKey = coord->getCurrentCoordinatesPlaceKey();
	int idx = ui->placeComboBox->findData(currentPlaceKey, Qt::UserRole, Qt::MatchCaseSensitive);
	if (idx==-1)
	{
		// Use TopRight as default
		idx = ui->placeComboBox->findData(QVariant("TopRight"), Qt::UserRole, Qt::MatchCaseSensitive);
	}
	ui->placeComboBox->setCurrentIndex(idx);
	setCustomCoordinatesAccess(currentPlaceKey);
	connect(ui->placeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setCoordinatesPlace(int)));

	populateCoordinateSystemsList();
	idx = ui->coordinateSystemComboBox->findData(coord->getCurrentCoordinateSystemKey(), Qt::UserRole, Qt::MatchCaseSensitive);
	if (idx==-1)
	{
		// Use RaDecJ2000 as default
		idx = ui->coordinateSystemComboBox->findData(QVariant("RaDecJ2000"), Qt::UserRole, Qt::MatchCaseSensitive);
	}
	ui->coordinateSystemComboBox->setCurrentIndex(idx);
	connect(ui->coordinateSystemComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setCoordinateSystem(int)));

	populateValues();
	connect(ui->spinBoxX, SIGNAL(valueChanged(int)), this, SLOT(setCustomCoordinatesPlace()));
	connect(ui->spinBoxY, SIGNAL(valueChanged(int)), this, SLOT(setCustomCoordinatesPlace()));

	connect(ui->pushButtonSave, SIGNAL(clicked()), this, SLOT(saveCoordinatesSettings()));	
	connect(ui->pushButtonReset, SIGNAL(clicked()), this, SLOT(resetCoordinatesSettings()));

	// About tab
	setAboutHtml();
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if(gui!=Q_NULLPTR)
		ui->aboutTextBrowser->document()->setDefaultStyleSheet(QString(gui->getStelStyle().htmlStyleSheet));
}

void PointerCoordinatesWindow::populateValues()
{
	QPair<int, int> cc = coord->getCustomCoordinatesPlace();
	ui->spinBoxX->setValue(cc.first);
	ui->spinBoxY->setValue(cc.second);
}

void PointerCoordinatesWindow::setAboutHtml(void)
{
	QString html = "<html><head></head><body>";
	html += "<h2>" + q_("Pointer Coordinates plug-in") + "</h2><table width=\"90%\">";
	html += "<tr width=\"30%\"><td><strong>" + q_("Version") + ":</strong></td><td>" + POINTERCOORDINATES_PLUGIN_VERSION + "</td></tr>";
	html += "<tr><td><strong>" + q_("License") + ":</strong></td><td>" + POINTERCOORDINATES_PLUGIN_LICENSE + "</td></tr>";
	html += "<tr><td><strong>" + q_("Author") + ":</strong></td><td>Alexander Wolf</td></tr>";
	html += "<tr><td><strong>" + q_("Contributors") + ":</strong></td><td>Georg Zotti</td></tr>";
	html += "</table>";

	html += "<p>" + q_("Show coordinates of the mouse cursor on the screen.");
	html += "</p>";

	html += StelApp::getInstance().getModuleMgr().getStandardSupportLinksInfo("Pointer Coordinates plugin");
	html += "</body></html>";

	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if(gui!=Q_NULLPTR)
	{
		QString htmlStyleSheet(gui->getStelStyle().htmlStyleSheet);
		ui->aboutTextBrowser->document()->setDefaultStyleSheet(htmlStyleSheet);
	}

	ui->aboutTextBrowser->setHtml(html);
}

void PointerCoordinatesWindow::saveCoordinatesSettings()
{
	coord->saveConfiguration();
}

void PointerCoordinatesWindow::resetCoordinatesSettings()
{
	if (askConfirmation())
	{
		qDebug() << "[PointerCoordinates] restore defaults...";
		coord->restoreDefaultConfiguration();
		populateValues();
	}
	else
		qDebug() << "[PointerCoordinates] restore defaults is canceled...";
}

void PointerCoordinatesWindow::populateCoordinatesPlacesList()
{
	Q_ASSERT(ui->placeComboBox);

	QComboBox* places = ui->placeComboBox;

	//Save the current selection to be restored later
	places->blockSignals(true);
	int index = places->currentIndex();
	QVariant selectedPlaceId = places->itemData(index);	
	places->clear();
	//For each algorithm, display the localized name and store the key as user
	//data. Unfortunately, there's no other way to do this than with a cycle.
	places->addItem(q_("The top center of the screen"), "TopCenter");
	places->addItem(q_("In center of the top right half of the screen"), "TopRight");
	places->addItem(q_("The right bottom corner of the screen"), "RightBottomCorner");
	places->addItem(q_("Near mouse cursor"), "NearMouseCursor");
	places->addItem(q_("Custom position"), "Custom");

	//Restore the selection
	index = places->findData(selectedPlaceId, Qt::UserRole, Qt::MatchCaseSensitive);
	places->setCurrentIndex(index);
	places->blockSignals(false);	
}

void PointerCoordinatesWindow::populateCoordinateSystemsList()
{
	Q_ASSERT(ui->coordinateSystemComboBox);

	QComboBox* csys = ui->coordinateSystemComboBox;

	//Save the current selection to be restored later
	csys->blockSignals(true);
	int index = csys->currentIndex();
	QVariant selectedSystemId = csys->itemData(index);
	csys->clear();
	//For each algorithm, display the localized name and store the key as user
	//data. Unfortunately, there's no other way to do this than with a cycle.
	csys->addItem(q_("Right ascension/Declination (J2000.0)"), "RaDecJ2000");
	csys->addItem(q_("Right ascension/Declination"), "RaDec");
	csys->addItem(q_("Hour angle/Declination"), "HourAngle");
	csys->addItem(q_("Ecliptic Longitude/Latitude"), "Ecliptic");
	csys->addItem(q_("Ecliptic Longitude/Latitude (J2000.0)"), "EclipticJ2000");
	csys->addItem(q_("Altitude/Azimuth"), "AltAzi");
	csys->addItem(q_("Galactic Longitude/Latitude"), "Galactic");
	csys->addItem(q_("Supergalactic Longitude/Latitude"), "Supergalactic");

	//Restore the selection
	index = csys->findData(selectedSystemId, Qt::UserRole, Qt::MatchCaseSensitive);
	csys->setCurrentIndex(index);
	csys->blockSignals(false);
}

void PointerCoordinatesWindow::setCoordinatesPlace(int placeID)
{
	QString currentPlaceID = ui->placeComboBox->itemData(placeID).toString();
	coord->setCurrentCoordinatesPlaceKey(currentPlaceID);
	setCustomCoordinatesAccess(currentPlaceID);
}

void PointerCoordinatesWindow::setCoordinateSystem(int csID)
{
	QString currentCsId = ui->coordinateSystemComboBox->itemData(csID).toString();
	coord->setCurrentCoordinateSystemKey(currentCsId);
}

void PointerCoordinatesWindow::setCustomCoordinatesPlace()
{
	coord->setCustomCoordinatesPlace(ui->spinBoxX->value(), ui->spinBoxY->value());
}

void PointerCoordinatesWindow::setCustomCoordinatesAccess(QString place)
{
	if (place.contains("Custom"))
	{
		ui->labelCustomCoords->setText(q_("Coordinates of custom position:"));
		ui->spinBoxX->setVisible(true);
		ui->spinBoxY->setVisible(true);
	}
	else
	{
		ui->labelCustomCoords->setText("");
		ui->spinBoxX->setVisible(false);
		ui->spinBoxY->setVisible(false);
	}
}
