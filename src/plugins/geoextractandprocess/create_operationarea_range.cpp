#include "create_operationarea_range.h"

#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"

#include <QFileDialog>
#include <QMessageBox>

QgsCreateOperationAreaRange::QgsCreateOperationAreaRange(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_Save, &QPushButton::clicked, this, &QgsCreateOperationAreaRange::Button_Save_clicked);
	//connect(ui.Button_Open, &QPushButton::clicked, this, &QgsCreateOperationAreaRange::Button_Open_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsCreateOperationAreaRange::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsCreateOperationAreaRange::Button_Cancel_rejected);
	//connect(ui.radioButton_File, &QRadioButton::clicked, this, &QgsCreateOperationAreaRange::RadioButton_File_clicked);
	//connect(ui.radioButton_InputRect, &QRadioButton::clicked, this, &QgsCreateOperationAreaRange::RadioButton_InputRect_clicked);

	restoreState();
	//ui.lineEdit_InputFilePath->setText(m_InputLayerPath);
	ui.lineEdit_SavePath->setText(m_LayerSavePath);

	InitLineEdit();

	ui.comboBox_Scale->setCurrentIndex(0);

	// 默认状态选择从文件获取范围
	//ui.radioButton_File->setChecked(true);
	ui.lineEdit_Left->setEnabled(true);
	ui.lineEdit_Top->setEnabled(true);
	ui.lineEdit_Right->setEnabled(true);
	ui.lineEdit_Bottom->setEnabled(true);
}

QgsCreateOperationAreaRange::~QgsCreateOperationAreaRange()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("CreateOperationAreaRange/InputLayerPath"), m_InputLayerPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CreateOperationAreaRange/LayerSavePath"), m_LayerSavePath, QgsSettings::Section::Plugins);
}

QString QgsCreateOperationAreaRange::GetLayerSavePath()
{
	return m_LayerSavePath;
}

QString QgsCreateOperationAreaRange::GetInputFilePath()
{
	return m_InputLayerPath;
}

double QgsCreateOperationAreaRange::GetScale()
{
	// 1:5万
	if (ui.comboBox_Scale->currentIndex() == 0)
	{
		return 50000;
	}

	// 1:25万
	if (ui.comboBox_Scale->currentIndex() == 1)
	{
		return 250000;
	}

	// 1:100万
	if (ui.comboBox_Scale->currentIndex() == 2)
	{
		return 1000000;
	}
	return 0;
}

int QgsCreateOperationAreaRange::GetInputType()
{
	/*if (ui.radioButton_File->isChecked())
	{
		return 1;
	}
	else
	{
		return 2;
	}*/
	return 2;
}

void QgsCreateOperationAreaRange::GetOperationAreaRange(double& dLeft, double& dTop, double& dRight, double& dBottom)
{
	QString qstrLeft = ui.lineEdit_Left->text();
	dLeft = qstrLeft.toDouble();

	QString qstrRight = ui.lineEdit_Right->text();
	dRight = qstrRight.toDouble();

	QString qstrTop = ui.lineEdit_Top->text();
	dTop = qstrTop.toDouble();

	QString qstrBottom = ui.lineEdit_Bottom->text();
	dBottom = qstrBottom.toDouble();
}

void QgsCreateOperationAreaRange::Button_Save_clicked()
{
	QString curPath = m_LayerSavePath; 
	QString dlgTitle = tr("保存文件");
	QString filter = "ESRI Shapefile(*.shp;*.SHP)";
	QString strFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);

	if (!strFileName.isEmpty())
	{
		QFile file(strFileName);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
			return;

		ui.lineEdit_SavePath->setText(strFileName);
		m_LayerSavePath = strFileName;
	}
}

void QgsCreateOperationAreaRange::Button_Open_clicked()
{
	QString curPath = m_InputLayerPath; 
	QString dlgTitle = tr("选择文件");
	QString filter = "ESRI Shapefile(*.shp;*.SHP)";
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	m_InputLayerPath = strFileName;
}

void QgsCreateOperationAreaRange::Button_OK_accepted()
{
	QFileInfo fileInfo(ui.lineEdit_SavePath->text());

	if (!FilePathIsExisted(fileInfo.absoluteDir().path()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("图层保存路径不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	accept();
}

void QgsCreateOperationAreaRange::Button_Cancel_rejected()
{
	reject();
}

void QgsCreateOperationAreaRange::Button_Help_showHelp()
{
	// TODO:增加帮助文档
	//QgsHelp::openHelp( QStringLiteral( "plugins/core_plugins/plugins_offline_editing.html" ) );
}

void QgsCreateOperationAreaRange::RadioButton_File_clicked()
{
	//ui.lineEdit_InputFilePath->setEnabled(true);
	//ui.Button_Open->setEnabled(true);
	ui.lineEdit_Left->setEnabled(false);
	ui.lineEdit_Top->setEnabled(false);
	ui.lineEdit_Right->setEnabled(false);
	ui.lineEdit_Bottom->setEnabled(false);
}

void QgsCreateOperationAreaRange::RadioButton_InputRect_clicked()
{
	//ui.lineEdit_InputFilePath->setEnabled(false);
	//ui.Button_Open->setEnabled(false);
	ui.lineEdit_Left->setEnabled(true);
	ui.lineEdit_Top->setEnabled(true);
	ui.lineEdit_Right->setEnabled(true);
	ui.lineEdit_Bottom->setEnabled(true);
}

void QgsCreateOperationAreaRange::restoreState()
{
	const QgsSettings settings;
	m_InputLayerPath = settings.value(QStringLiteral("CreateOperationAreaRange/InputLayerPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_LayerSavePath = settings.value(QStringLiteral("CreateOperationAreaRange/LayerSavePath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}


void QgsCreateOperationAreaRange::InitLineEdit()
{
	// 上边界纬度
	ui.lineEdit_Top->setValidator(new QRegExpValidator(QRegExp("^-?(90|[1-8]?\\d(\\.\\d+)?)$")));

	// 下边界纬度
	ui.lineEdit_Bottom->setValidator(new QRegExpValidator(QRegExp("^-?(90|[1-8]?\\d(\\.\\d+)?)$")));

	// 左边界经度
	ui.lineEdit_Left->setValidator(new QRegExpValidator(QRegExp("^-?(180|1?[0-7]?\\d(\\.\\d+)?)$")));

	// 右边界经度
	ui.lineEdit_Right->setValidator(new QRegExpValidator(QRegExp("^-?(180|1?[0-7]?\\d(\\.\\d+)?)$")));

}


// 判断目录是否存在
bool QgsCreateOperationAreaRange::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool QgsCreateOperationAreaRange::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}