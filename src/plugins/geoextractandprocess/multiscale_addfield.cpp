#include "multiscale_addfield.h"

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

QgsPreProcessMultiScaleDataDlg::QgsPreProcessMultiScaleDataDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.pushButton_Open, &QPushButton::clicked, this, &QgsPreProcessMultiScaleDataDlg::Button_Open_clicked);
	connect(ui.pushButton_Save, &QPushButton::clicked, this, &QgsPreProcessMultiScaleDataDlg::Button_Save_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsPreProcessMultiScaleDataDlg::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsPreProcessMultiScaleDataDlg::Button_Cancel_rejected);

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputPath);
	ui.lineEdit_OutputDataPath->setText(m_qstrOutputPath);
}

QgsPreProcessMultiScaleDataDlg::~QgsPreProcessMultiScaleDataDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("GeoExtractAndProcess/preprocessmultiscale_input_path"), m_qstrInputPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("GeoExtractAndProcess/preprocessmultiscale_output_path"), m_qstrOutputPath, QgsSettings::Section::Plugins);
}

QString QgsPreProcessMultiScaleDataDlg::GetInputPath()
{
	return m_qstrInputPath;
}

QString QgsPreProcessMultiScaleDataDlg::GetOutputPath()
{
	return m_qstrOutputPath;
}

void QgsPreProcessMultiScaleDataDlg::Button_Open_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrInputPath;
	QString dlgTile = QObject::tr("请选择图幅所在路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputDataPath->setText(selectedDir);
		m_qstrInputPath = selectedDir;
	}
}

void QgsPreProcessMultiScaleDataDlg::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputPath;
	QString dlgTile = QObject::tr("请选择输出数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputDataPath->setText(selectedDir);
		m_qstrOutputPath = selectedDir;
	}
}

void QgsPreProcessMultiScaleDataDlg::Button_OK_accepted()
{
	// 检查输入目录有效性

	if (!FilePathIsExisted(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("输入数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(ui.lineEdit_OutputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("输出数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	accept();
}

void QgsPreProcessMultiScaleDataDlg::Button_Cancel_rejected()
{
	reject();
}

void QgsPreProcessMultiScaleDataDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputPath = settings.value(QStringLiteral("GeoExtractAndProcess/preprocessmultiscale_input_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputPath = settings.value(QStringLiteral("GeoExtractAndProcess/preprocessmultiscale_output_path"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}


bool QgsPreProcessMultiScaleDataDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

bool QgsPreProcessMultiScaleDataDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}