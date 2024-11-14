#include "copy_shapefiles.h"

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
#include <QtGui\qvalidator.h>

#include <gdal.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>
#include <gdal_version.h>
#include <cpl_error.h>
#include <cpl_string.h>
#include "qgsogrutils.h"
#include <tchar.h>
#include <ogrsf_frmts.h>
#include <ogr_feature.h>
#include <gdal_priv.h>

#include "cse_geoextractandprocess.h"
#include <string>
using namespace std;


QgsCopyShapefilesDlg::QgsCopyShapefilesDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_Save, &QPushButton::clicked, this, &QgsCopyShapefilesDlg::Button_Save_clicked);
	connect(ui.Button_Open, &QPushButton::clicked, this, &QgsCopyShapefilesDlg::Button_Open_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsCopyShapefilesDlg::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsCopyShapefilesDlg::Button_Cancel_rejected);

	restoreState();

	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
}

QgsCopyShapefilesDlg::~QgsCopyShapefilesDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("CopyShapefiles/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("CopyShapefiles/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
}

void QgsCopyShapefilesDlg::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrOutputDataPath;
	QString dlgTile = QObject::tr("请选择目的存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputDataPath->setText(selectedDir);
		m_qstrOutputDataPath = selectedDir;
	}
}

void QgsCopyShapefilesDlg::Button_Open_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = QObject::tr("请选择矢量数据源路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputDataPath->setText(selectedDir);
		m_qstrInputDataPath = selectedDir;
	}
}

void QgsCopyShapefilesDlg::Button_OK_accepted()
{
	ui.progressBar->setValue(0);
	// 输入数据路径
	QString qstrInputDataPath = ui.lineEdit_InputDataPath->text();

	// 输出数据路径
	QString qstrOutputDataPath = ui.lineEdit_OutputDataPath->text();

	if (!FilePathIsExisted(qstrInputDataPath))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("矢量数据源目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(qstrOutputDataPath))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("目的存储目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 获取当前路径下所有文件或文件夹
	QStringList qstrPathList = GetDirPathOfSplDir(qstrInputDataPath);
	
	// 如果当前选择单个图幅文件夹目录
	if (qstrPathList.size() == 0)
	{
		string strInputDataPath = qstrInputDataPath.toLocal8Bit();
		string strOutputDataPath = qstrOutputDataPath.toLocal8Bit();
		
		int iResult = CSE_GeoExtractAndProcess::CopyShapefiles(strInputDataPath, strOutputDataPath);
		if (iResult != 0)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("矢量数据迁移失败！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}
		else
		{
			ui.progressBar->setValue(100);
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("地理提取与加工"));
			msgBox.setText(tr("矢量数据迁移完成！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}
	}
	// 如果是多个图幅所在文件夹
	else
	{
		for (int i = 0; i < qstrPathList.size(); i++)
		{
			// 每个子目录
			string strInputDataPath = qstrPathList[i].toLocal8Bit();
			string strOutputDataPath = qstrOutputDataPath.toLocal8Bit();

			int iResult = CSE_GeoExtractAndProcess::CopyShapefiles(strInputDataPath, strOutputDataPath);
			if (iResult != 0)
			{
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("地理提取与加工"));
				msgBox.setText(tr("矢量数据迁移失败！"));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}
		
			// 设置进度条
			ui.progressBar->setValue(int(100.0 * (int(i) + 1) / qstrPathList.size()));
		}

		ui.progressBar->setValue(100);

		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("矢量数据迁移完成！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();

		return;
	}

	accept();
}

void QgsCopyShapefilesDlg::Button_Cancel_rejected()
{
	reject();
}

void QgsCopyShapefilesDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("CopyShapefiles/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("CopyShapefiles/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

QString QgsCopyShapefilesDlg::GetInputDataPath()
{
	m_qstrInputDataPath = ui.lineEdit_InputDataPath->text();
	return m_qstrInputDataPath;
}

QString QgsCopyShapefilesDlg::GetOutputDataPath()
{
	return m_qstrOutputDataPath;
}



// 判断目录是否存在
bool QgsCopyShapefilesDlg::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool QgsCopyShapefilesDlg::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*获取指定目录下的文件夹个数*/
int QgsCopyShapefilesDlg::GetSubdirCountInSplDir(QString dirPath)
{
	return QDir(dirPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
}

/*获取在指定目录下的目录的路径*/
QStringList QgsCopyShapefilesDlg::GetDirPathOfSplDir(QString dirPath)
{
	QStringList dirPaths;
	QDir splDir(dirPath);
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	QFileInfo tempFileInfo;
	foreach(tempFileInfo, fileInfoListInSplDir) {
		dirPaths << tempFileInfo.absoluteFilePath();
	}
	return dirPaths;
}
