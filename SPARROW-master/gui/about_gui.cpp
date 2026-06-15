#ifdef _WIN32  
#include <windows.h>
#elif __linux 
#include <cstring>
#include <stdio.h> 
#define fopen_s(pFile,filename,mode) ((*(pFile))=fopen((filename),  (mode)))==NULL
#endif 
#include <fstream>
#include <QMessageBox>
#include <QLabel>
#include <QFileDialog>
#include <QDateTime>
#include <iostream>
#include <sys/stat.h>
#include "protocol.h"
#include "about_gui.h"
#include "version.h"
#include "cameracapturegui.h"

AboutGui::AboutGui(QWidget* parent)
	: QDialog(parent)
{
	ui.setupUi(this);
}

AboutGui::~AboutGui()
{
}

void AboutGui::setFirmwareVersion(QString version)
{
	firmware_version = version;
}

void AboutGui::updateVersion()
{
	ui.label_client_verson->setText(_VERSION_NUMBER_);
	ui.label_firmware_version->setText(firmware_version);
}

void AboutGui::setProductInfo(QString info)
{
	info = "";
	product_info = info;
}

void AboutGui::updateProductInfo()
{
	ui.textBrowser->setText(product_info);
}

void AboutGui::print_log(QString str)
{
	QString log = str;

	ui.textBrowser->append(log);
	ui.textBrowser->repaint();
}
