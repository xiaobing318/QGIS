#include "DLHJ_Copilot_Chat.h"
/*--------------QGIS---------------*/
#include "qgssettings.h"
#include "qgsgui.h"
#include "qgsstatusbar.h"

/*--------------QT---------------*/
#include <QFileDialog>
#include <QMessageBox>
#include <qvalidator.h>



DLHJ_Copilot_Chat_Dialog::DLHJ_Copilot_Chat_Dialog(QgisInterface* qgisInterface, QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
	, m_qgisInterface(qgisInterface)
{

	connect(ui.Button_open, &QPushButton::clicked, this, &DLHJ_Copilot_Chat_Dialog::Button_open_accepted);
	connect(ui.Button_cancel, &QPushButton::clicked, this, &DLHJ_Copilot_Chat_Dialog::Button_open_accepted);

}

DLHJ_Copilot_Chat_Dialog::~DLHJ_Copilot_Chat_Dialog()
{

}


//	打开
void DLHJ_Copilot_Chat_Dialog::Button_open_accepted()
{
	QDesktopServices::openUrl(QUrl(QString("http://202.107.245.41:1203/#/sliceMana")));
}

//  关闭
void DLHJ_Copilot_Chat_Dialog::Button_cancel_rejected()
{
	return;
}

void DLHJ_Copilot_Chat_Dialog::restoreState()
{
	const QgsSettings settings;
}


