#include "convert_operationarea_and_sheet.h"

#include "qgshelp.h"
#include "qgsmaplayer.h"
#include "qgsproject.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"

#include <QFileDialog>
#include <QMessageBox>

#include "cse_geoextractandprocess.h"

QgsConvertOperationAreaAndSheet::QgsConvertOperationAreaAndSheet(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_Convert, &QPushButton::clicked, this, &QgsConvertOperationAreaAndSheet::Button_Convert_accepted);
	connect(ui.Button_Close, &QPushButton::clicked, this, &QgsConvertOperationAreaAndSheet::Button_Close_rejected);

	ui.radioButton_OperationArea->setChecked(true);
}

QgsConvertOperationAreaAndSheet::~QgsConvertOperationAreaAndSheet()
{
}

void QgsConvertOperationAreaAndSheet::Button_Convert_accepted()
{
	ui.textEdit_OutputName->clear();
	int iResult = 0;
	// 如果选择作业区
	if (ui.radioButton_OperationArea->isChecked())
	{
		QString qstrInputName = ui.lineEdit_InputName->text();
		string strInputName = qstrInputName.toStdString();

		vector<string> vSheetList;
		vSheetList.clear();
		iResult = CSE_GeoExtractAndProcess::GetSheetListByOperationAreaName(strInputName, vSheetList);
		if (iResult != 0)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("作业区获取图幅号失败！"));
			msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
			msgBox.setDefaultButton(QMessageBox::Cancel);
			msgBox.exec();
			return;
		}

		for (int i = 0; i < vSheetList.size(); i++)
		{
			string strSheet = vSheetList[i] + "\n";
			QString qstrSheet = QString::fromStdString(strSheet);
			ui.textEdit_OutputName->insertPlainText(qstrSheet);
		}
		QString qstrOutput;
		qstrOutput.sprintf("输出图幅：共%d个图幅", vSheetList.size());
		ui.label_output->setText(qstrOutput);
	}
	// 如果选择图幅号
	else
	{
		QString qstrOutput;
		qstrOutput.sprintf("输出作业区名称");
		ui.label_output->setText(qstrOutput);

		QString qstrInputName = ui.lineEdit_InputName->text();
		string strInputName = qstrInputName.toStdString();

		string strOpName;
		iResult = CSE_GeoExtractAndProcess::GetOperationAreaNameBySheet(strInputName, strOpName);
		if (iResult != 0)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("图幅号获取作业区名称失败！"));
			msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
			msgBox.setDefaultButton(QMessageBox::Cancel);
			msgBox.exec();
			return;
		}

		strOpName += "\n";
		QString qstrOpName = QString::fromStdString(strOpName);
		ui.textEdit_OutputName->insertPlainText(qstrOpName);
	}
}

void QgsConvertOperationAreaAndSheet::Button_Close_rejected()
{
	reject();
}