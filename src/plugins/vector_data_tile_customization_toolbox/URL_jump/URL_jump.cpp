#include "URL_jump.h"
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>

/*-------------------------------*/
#ifdef OS_FAMILY_WINDOWS
#include "windows.h"
#else
#include "unistd.h"
#endif


url_jump_dialog::url_jump_dialog(QgisInterface* qgisInterface, QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
	, m_qgisInterface(qgisInterface)
{

	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);

	connect(ui.Button_open, &QPushButton::clicked, this, &url_jump_dialog::Button_open_accepted);
	connect(ui.Button_cancel, &QPushButton::clicked, this, &url_jump_dialog::Button_open_accepted);

}

url_jump_dialog::~url_jump_dialog()
{

}


//	打开
void url_jump_dialog::Button_open_accepted()
{
	QDesktopServices::openUrl(QUrl(QString("http://202.107.245.41:1203/#/sliceMana")));
}

//  关闭
void url_jump_dialog::Button_cancel_rejected()
{
	return;
}

void url_jump_dialog::restoreState()
{
	const QgsSettings settings;
}


