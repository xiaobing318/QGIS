#define _HAS_STD_BYTE 0
#include "se_manage_ARD_package.h"
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
#include <qcheckbox.h>
#include <qmenu.h>

#include <QFile>
#include "cpl_config.h"

#include "gdal_priv.h"
#include <ogrsf_frmts.h>
#include <ogr_feature.h>

#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif //
#define _atoi64(val)   strtoll(val,NULL,10)

/*--------------STL---------------*/
#include <filesystem>
#include <fstream>
#include <iostream>


/*--------------STL---------------*/


using namespace std;

SE_ManageARDPackageDlg::SE_ManageARDPackageDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK, &QPushButton::clicked, this, &SE_ManageARDPackageDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &SE_ManageARDPackageDlg::pushButton_Cancel_clicked);
	connect(ui.pushButton_Query, &QPushButton::clicked, this, &SE_ManageARDPackageDlg::pushButton_Query_clicked);
	connect(ui.lineEdit_InputDataPath, &QLineEdit::textChanged, this, &SE_ManageARDPackageDlg::on_lineEdit_InputDataPath_TextChanged);
	connect(ui.pushButton_Open, &QPushButton::clicked, this, &SE_ManageARDPackageDlg::pushButton_Open_clicked);
	// 设置右键菜单
	ui.treeWidget_ARD_Package->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.treeWidget_ARD_Package, &QTreeWidget::customContextMenuRequested, this, &SE_ManageARDPackageDlg::showContextMenu);

	restoreState();
	
	ui.lineEdit_ARD_PackageName->setValidator(new QRegExpValidator(QRegExp("^[a-zA-Z0-9._-]*$"), this));
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);

	// 颜色交叉
	ui.treeWidget_ARD_Package->setAlternatingRowColors(true);

	// 默认是矢量分析就绪型数据包
	m_bVectorGpkgPackage = true;

}

SE_ManageARDPackageDlg::~SE_ManageARDPackageDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("MANAGE_ARD_PACKAGE/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);

}

void SE_ManageARDPackageDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("MANAGE_ARD_PACKAGE/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}


/*加载配置文件*/
void SE_ManageARDPackageDlg::pushButton_Open_clicked()
{
	ui.treeWidget_ARD_Package->clear();

	// 选择文件夹
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = tr("请选择分析就绪型数据包路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputDataPath->setText(selectedDir);
		m_qstrInputDataPath = selectedDir;

		// 填充树控件
		InitTreeWidget();

	}
}

void SE_ManageARDPackageDlg::pushButton_OK_clicked()
{
	// 如果文件夹不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件夹：%1不存在！").arg(ui.lineEdit_InputDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("MANAGE_ARD_PACKAGE/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("MANAGE_ARD_PACKAGE/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	accept();
}




void SE_ManageARDPackageDlg::GetPackageInfosFromTreeWidget(vector<QString>& vInfos)
{
	vInfos.clear();

	// 遍历树控件的所有项
	for (int i = 0; i < ui.treeWidget_ARD_Package->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* topItem = ui.treeWidget_ARD_Package->topLevelItem(i);

		if (topItem)
		{
			// 数据包标识
			vInfos.push_back(topItem->text(0));
		}
	}
}

void SE_ManageARDPackageDlg::pushButton_Query_clicked()
{
	// 获取树控件的任务列表
	vector<QString> vInfos;
	vInfos.clear();
	GetPackageInfosFromTreeWidget(vInfos);

	// 满足条件的任务个数
	int iResultCount = 0;

	for (int i = 0; i < vInfos.size(); ++i)
	{
		// 如果包含任务名称
		if (vInfos[i].contains(ui.lineEdit_ARD_PackageName->text()))
		{
			ui.treeWidget_ARD_Package->topLevelItem(i)->setBackground(0, QColor(173, 216, 230)); // Light Blue
			ui.treeWidget_ARD_Package->topLevelItem(i)->setBackground(1, QColor(173, 216, 230)); // Light Blue
			iResultCount++;
		}
		else
		{
			ui.treeWidget_ARD_Package->topLevelItem(i)->setBackground(0, QColor(255, 255, 255)); // 白色
			ui.treeWidget_ARD_Package->topLevelItem(i)->setBackground(1, QColor(255, 255, 255)); // 白色
		}
	}

	// 查询结果弹窗
	if (iResultCount == 0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("满足查询条件的数据包个数为0！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}
}

void SE_ManageARDPackageDlg::pushButton_Cancel_clicked()
{
	reject();
}




void SE_ManageARDPackageDlg::InitTreeWidget()
{
	// 遍历当前文件夹下所有的gpkg文件
	QStringList qstrGpkgNameList = GetGpkgFileNames(m_qstrInputDataPath);

	// 如果当前数据目录下没有gpkg文件，提示
	if (qstrGpkgNameList.length() == 0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("当前目录：%1下无gpkg数据包，请重新选择数据包路径！").arg(m_qstrInputDataPath));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 读取第1个gpkg文件，判断是栅格分析就绪数据包还是矢量分析就绪数据包
	// 如果获取的ogrlayer为0，则当前数据包为栅格分析就绪型数据包
	QString qstrGpkgFileNameOfIndexZero = m_qstrInputDataPath + "/" + qstrGpkgNameList[0];
	string strGpkgFileName = qstrGpkgFileNameOfIndexZero.toLocal8Bit();

	m_bVectorGpkgPackage = true;
	GDALAllRegister();

	// 打开gpkg文件
	GDALDataset* poGpkgDataset = (GDALDataset*)GDALOpenEx(strGpkgFileName.c_str(), GDAL_OF_UPDATE, nullptr, nullptr, nullptr);
	
	// 如果gpkg数据对象为空，则gpkg包为栅格分析就绪数据包
	if (poGpkgDataset == nullptr) {
		//std::cerr << "Failed to open gpkg file: " << geojsonFile << std::endl;
		m_bVectorGpkgPackage = false;
	}
	else
	{
		m_bVectorGpkgPackage = true;
		GDALClose(poGpkgDataset);
	}

	// 如果是矢量分析就绪型数据包
	if (m_bVectorGpkgPackage)
	{
		// 创建顶层节点，即当前文件夹下所有的gpkg文件名
		for (int i = 0; i < qstrGpkgNameList.length(); ++i)
		{
			// 从每个gpkg数据包元数据中获取信息并填充到树控件中
			string strGpkgFullPath = m_qstrInputDataPath.toLocal8Bit();
			strGpkgFullPath += "/";
			strGpkgFullPath += qstrGpkgNameList[i].toLocal8Bit();

			// 从gpkg数据包中读取metadata表格，获取表格中所有信息填入树控件中
			ARD_Package_MetaData_Info info;
			if (!GetMetaDataInVectorGpkg(strGpkgFileName, info))
			{
				continue;
			}

			// gpkg数据包名
			string strGpkgName = CPLGetBasename(strGpkgFullPath.c_str());

			QList<QString> allItemName{ QString::fromLocal8Bit(strGpkgName.c_str()) };
			// 创建顶层Item-数据包名称
			QTreeWidgetItem* ARD_Package_Item = new QTreeWidgetItem(allItemName);
			ARD_Package_Item->setFlags(ARD_Package_Item->flags() | Qt::ItemIsEditable); // 设置子项可编辑
			ui.treeWidget_ARD_Package->addTopLevelItem(ARD_Package_Item);

			// 插入多个模型
			for (int iMetaDataIndex = 0; iMetaDataIndex < info.vIdentifys.size(); ++iMetaDataIndex)
			{
				QString qstrIdentify = QString::fromUtf8(info.vIdentifys[iMetaDataIndex].c_str());
				QString qstrName = QString::fromUtf8(info.vNames[iMetaDataIndex].c_str());
				QString qstrLevel = QString::number(info.vDataLevels[iMetaDataIndex]);
				QString qstrType = QString::fromUtf8(info.vDataTypes[iMetaDataIndex].c_str());

				QString qstrLeft = QString::number(info.vdRects[iMetaDataIndex].dleft);
				QString qstrTop = QString::number(info.vdRects[iMetaDataIndex].dtop);
				QString qstrRight = QString::number(info.vdRects[iMetaDataIndex].dright);
				QString qstrBottom = QString::number(info.vdRects[iMetaDataIndex].dbottom);

				QString qstrDetail = QString::fromUtf8(info.vDetails[iMetaDataIndex].c_str());
				QString qstrTask = QString::fromUtf8(info.vTasks[iMetaDataIndex].c_str());
				QString qstrTemporal = QString::fromUtf8(info.vTemporals[iMetaDataIndex].c_str());




				// 创建模型子节点
				QTreeWidgetItem* childItem = new QTreeWidgetItem(QStringList{
					qstrIdentify,
					qstrName,
					qstrLevel,
					qstrType,
					qstrLeft,
					qstrTop,
					qstrRight,
					qstrBottom,
					qstrDetail,
					qstrTask,
					qstrTemporal });


				childItem->setFlags(childItem->flags() | Qt::ItemIsEditable); // 设置子项可编辑

				for (int iColIndex = 0; iColIndex < ui.treeWidget_ARD_Package->columnCount(); ++iColIndex)
				{
					childItem->setTextAlignment(iColIndex, Qt::AlignCenter); // 设置子项居中
				}

				// 将模型子节点添加到顶层节点上
				ARD_Package_Item->addChild(childItem);

				// 创建按钮
				QPushButton* buttonView = new QPushButton("查看...");
				buttonView->setFixedSize(80, 30); // 设置按钮的固定大小

				// 创建一个 QWidget 来放置按钮，并设置其大小
				QWidget* buttonWidget = new QWidget();
				buttonWidget->setFixedSize(100, 40); // 设置 QWidget 的大小

				// 使用布局将按钮居中
				QVBoxLayout* layout = new QVBoxLayout(buttonWidget);
				layout->addWidget(buttonView, 0, Qt::AlignCenter); // 将按钮居中
				layout->setContentsMargins(0, 0, 0, 0); // 去掉边距

				// 将按钮 QWidget 添加到 QTreeWidgetItem 的特定列
				ui.treeWidget_ARD_Package->setItemWidget(childItem, 11, buttonWidget); // 11 是列索引

				// 连接按钮点击事件
				QObject::connect(buttonView, &QPushButton::clicked, [=]() {
					QMessageBox msgBox;
					msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
					msgBox.setText(tr("%1").arg(qstrIdentify));
					msgBox.setStandardButtons(QMessageBox::Yes);
					msgBox.setDefaultButton(QMessageBox::Yes);
					msgBox.exec();
					return;
					});




			}

		}


		ui.treeWidget_ARD_Package->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
		// 设置标题默认居中
		ui.treeWidget_ARD_Package->header()->setDefaultAlignment(Qt::AlignHCenter);

	}

	// 如果是栅格分析就绪型数据包
	else
	{
		// 创建顶层节点，即当前文件夹下所有的gpkg文件名
		for (int i = 0; i < qstrGpkgNameList.length(); ++i)
		{
			// 从每个gpkg数据包元数据中获取信息并填充到树控件中
			string strGpkgFullPath = m_qstrInputDataPath.toLocal8Bit();
			strGpkgFullPath += "/";
			strGpkgFullPath += qstrGpkgNameList[i].toLocal8Bit();

			// 从gpkg数据包中读取metadata表格，获取表格中所有信息填入树控件中
			ARD_Package_MetaData_Info info;
			if (!GetMetaDataInRasterGpkg(strGpkgFileName, info))
			{
				continue;
			}

			// gpkg数据包名
			string strGpkgName = CPLGetBasename(strGpkgFullPath.c_str());

			QList<QString> allItemName{ QString::fromLocal8Bit(strGpkgName.c_str()) };
			// 创建顶层Item-数据包名称
			QTreeWidgetItem* ARD_Package_Item = new QTreeWidgetItem(allItemName);
			ARD_Package_Item->setFlags(ARD_Package_Item->flags() | Qt::ItemIsEditable); // 设置子项可编辑
			ui.treeWidget_ARD_Package->addTopLevelItem(ARD_Package_Item);

			// 插入多个模型
			for (int iMetaDataIndex = 0; iMetaDataIndex < info.vIdentifys.size(); ++iMetaDataIndex)
			{
				QString qstrIdentify = QString::fromUtf8(info.vIdentifys[iMetaDataIndex].c_str());
				QString qstrName = QString::fromUtf8(info.vNames[iMetaDataIndex].c_str());
				QString qstrLevel = QString::number(info.vDataLevels[iMetaDataIndex]);
				QString qstrType = QString::fromUtf8(info.vDataTypes[iMetaDataIndex].c_str());

				QString qstrLeft = QString::number(info.vdRects[iMetaDataIndex].dleft);
				QString qstrTop = QString::number(info.vdRects[iMetaDataIndex].dtop);
				QString qstrRight = QString::number(info.vdRects[iMetaDataIndex].dright);
				QString qstrBottom = QString::number(info.vdRects[iMetaDataIndex].dbottom);

				QString qstrDetail = QString::fromUtf8(info.vDetails[iMetaDataIndex].c_str());
				QString qstrTask = QString::fromUtf8(info.vTasks[iMetaDataIndex].c_str());
				QString qstrTemporal = QString::fromUtf8(info.vTemporals[iMetaDataIndex].c_str());

				


				// 创建模型子节点
				QTreeWidgetItem* childItem = new QTreeWidgetItem(QStringList{
					qstrIdentify,
					qstrName,
					qstrLevel,
					qstrType,
					qstrLeft,
					qstrTop,
					qstrRight,
					qstrBottom,
					qstrDetail,
					qstrTask,
					qstrTemporal });
					
				
				childItem->setFlags(childItem->flags() | Qt::ItemIsEditable); // 设置子项可编辑

				for (int iColIndex = 0; iColIndex < ui.treeWidget_ARD_Package->columnCount(); ++iColIndex)
				{
					childItem->setTextAlignment(iColIndex, Qt::AlignCenter); // 设置子项居中
				}
		
				// 将模型子节点添加到顶层节点上
				ARD_Package_Item->addChild(childItem);

				// 创建按钮
				QPushButton* buttonView = new QPushButton("查看...");
				buttonView->setFixedSize(80, 30); // 设置按钮的固定大小

				// 创建一个 QWidget 来放置按钮，并设置其大小
				QWidget* buttonWidget = new QWidget();
				buttonWidget->setFixedSize(100, 40); // 设置 QWidget 的大小

				// 使用布局将按钮居中
				QVBoxLayout* layout = new QVBoxLayout(buttonWidget);
				layout->addWidget(buttonView, 0, Qt::AlignCenter); // 将按钮居中
				layout->setContentsMargins(0, 0, 0, 0); // 去掉边距

				// 将按钮 QWidget 添加到 QTreeWidgetItem 的特定列
				ui.treeWidget_ARD_Package->setItemWidget(childItem, 11, buttonWidget); // 11 是列索引

				// 连接按钮点击事件
				QObject::connect(buttonView, &QPushButton::clicked, [=]() {
					QMessageBox msgBox;
					msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
					msgBox.setText(tr("%1").arg(qstrIdentify));
					msgBox.setStandardButtons(QMessageBox::Yes);
					msgBox.setDefaultButton(QMessageBox::Yes);
					msgBox.exec();
					return;
					});


				

			}

		}


		ui.treeWidget_ARD_Package->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
		// 设置标题默认居中
		ui.treeWidget_ARD_Package->header()->setDefaultAlignment(Qt::AlignHCenter);

	}

}


// 显示右键菜单
void SE_ManageARDPackageDlg::showContextMenu(const QPoint& pos)
{
	// 数据包级别—0；分析就绪型数据级别—1
	QTreeWidgetItem* item = ui.treeWidget_ARD_Package->itemAt(pos);
	if (item) 
	{
		// 获取item的级别
		int iItemLevel = GetItemLevel(item);

		// 如果是分析就绪数据级别
		if (iItemLevel == 1)
		{
			QMenu contextMenu(tr("编辑数据包数据"), this);

			QAction deleteAction("删除数据", this);
			connect(&deleteAction, &QAction::triggered, this, &SE_ManageARDPackageDlg::deletePackageChildNode);
			contextMenu.addAction(&deleteAction);

			contextMenu.exec(ui.treeWidget_ARD_Package->viewport()->mapToGlobal(pos));
		}
	}
}

/*删除分析就绪数据包下的就绪数据*/
void SE_ManageARDPackageDlg::deletePackageChildNode()
{
	/*弹出对话框确认是否删除*/
	QMessageBox::StandardButton box;
	box = QMessageBox::question(this, "提示", "确定要删除吗?", QMessageBox::Yes | QMessageBox::No);
	if (box == QMessageBox::No){
		return;
	}
		

	QList<QTreeWidgetItem*> selectedItems = ui.treeWidget_ARD_Package->selectedItems();
	if (!selectedItems.isEmpty())
	{
		QTreeWidgetItem* selectedItem = selectedItems.first(); // 获取第一个选中的节点

		// 记录选择的节点对应的数据包及数据名称
		QString qstrPackageName = selectedItem->parent()->text(0);
		string strPackageName = qstrPackageName.toLocal8Bit();
		QString qstrARDIdentify = selectedItem->text(0);
		string strARDIdentify = qstrARDIdentify.toLocal8Bit();

		// 就绪数据包全路径
		string strGpkgFullName = m_qstrInputDataPath.toLocal8Bit();
		strGpkgFullName += "/";
		strGpkgFullName += strPackageName;
		strGpkgFullName += ".gpkg";


		// 更新gpkg数据包
		// 如果是矢量分析就绪数据包，则从gpkg数据集中依次删除包含"VECTOR_{分析就绪数据标识}"的图层，同时更新metadata图层
		if (m_bVectorGpkgPackage)
		{
			if (!DeleteVectorLayerByARDIdentify(strGpkgFullName, strARDIdentify))
			{
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
				msgBox.setText(tr("删除%1就绪数据失败！").arg(qstrARDIdentify));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}
			else
			{
				// 删除选中的项及其所有子项
				deleteItem(selectedItem);

				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
				msgBox.setText(tr("成功删除%1就绪数据！").arg(qstrARDIdentify));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}
		}

		// 如果是栅格分析就绪数据包，则从gpkg数据库中删除"RASTER_{分析就绪数据标识}"的表格，同时更新metadata表
		else
		{
			if (!DeleteRasterByARDIdentify(strGpkgFullName, strARDIdentify))
			{
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
				msgBox.setText(tr("删除%1就绪数据失败！").arg(qstrARDIdentify));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}
			else
			{
				// 删除选中的项及其所有子项
				deleteItem(selectedItem);

				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
				msgBox.setText(tr("成功删除%1就绪数据！").arg(qstrARDIdentify));
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();
				return;
			}
		}






	}	

	


}



bool SE_ManageARDPackageDlg::contains(const vector<QString>& vec, const QString& str) 
{
	return std::find(vec.begin(), vec.end(), str) != vec.end();
}


QStringList SE_ManageARDPackageDlg::GetGpkgFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.gpkg" << "*.GPKG";

	// 查找当前目录下的所有符合条件的文件
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

	// 查找子目录中的文件
	QStringList subDirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	for (const QString& subDir : subDirs)
	{
		QString subDirPath = dir.filePath(subDir);
		QStringList subDirFiles = GetGpkgFileNames(subDirPath); // 递归调用
		files.append(subDirFiles); // 将子目录中的文件添加到结果中
	}

	return files;
}


bool SE_ManageARDPackageDlg::GetMetaDataInVectorGpkg(string strGpkgFileFullPath, ARD_Package_MetaData_Info& info)
{
	// 矢量分析就绪型数据包存储元数据表，从元数据表metadata中读取元数据信息
	// 注册所有的格式
	GDALAllRegister();

	// 打开gpkg文件
	GDALDataset* poGpkgDataset = (GDALDataset*)GDALOpenEx(strGpkgFileFullPath.c_str(), GDAL_OF_ALL, nullptr, nullptr, nullptr);
	if (poGpkgDataset == nullptr) {
		//std::cerr << "Failed to open GeoJSON file: " << geojsonFile << std::endl;
		return false;
	}

	// 获取metadata图层
	OGRLayer* pMetaDataLayer = poGpkgDataset->GetLayerByName("metadata");
	if (!pMetaDataLayer)
	{
		// 关闭并释放数据集资源
		GDALClose(poGpkgDataset);
		// 记录日志：获取元数据图层失败，不存在metadata图层
		return false;
	}

	// 重置图层的读取指针到起始位置
	pMetaDataLayer->ResetReading();

	// 遍历图层中的所有要素（即元数据文件中的每一行）
	OGRFeature* poFeature = nullptr;

	while ((poFeature = pMetaDataLayer->GetNextFeature()) != NULL)
	{
		// 分析就绪数据标识
		info.vIdentifys.push_back(poFeature->GetFieldAsString(0));

		// 分析就绪数据名称
		info.vNames.push_back(poFeature->GetFieldAsString(1));

		// 数据级别
		info.vDataLevels.push_back(poFeature->GetFieldAsInteger(2));

		// 数据类型
		info.vDataTypes.push_back(poFeature->GetFieldAsString(3));

		SE_DRect dRect;

		// 左边界经度
		dRect.dleft = poFeature->GetFieldAsDouble(4);

		// 上边界经度
		dRect.dtop = poFeature->GetFieldAsDouble(5);

		// 右边界经度
		dRect.dright = poFeature->GetFieldAsDouble(6);

		// 下边界经度
		dRect.dbottom = poFeature->GetFieldAsDouble(7);

		info.vdRects.push_back(dRect);

		// 描述信息
		info.vDetails.push_back(poFeature->GetFieldAsString(8));

		// 支撑的任务
		info.vTasks.push_back(poFeature->GetFieldAsString(9));

		// 时相信息
		info.vTemporals.push_back(poFeature->GetFieldAsString(10));

		// 释放要素资源
		OGRFeature::DestroyFeature(poFeature);
	}

	// 关闭并释放数据集资源
	GDALClose(poGpkgDataset);



	return true;
}

bool SE_ManageARDPackageDlg::GetMetaDataInRasterGpkg(string strGpkgFileFullPath, ARD_Package_MetaData_Info& info)
{
	sqlite3* pDB = nullptr;
	if (sqlite3_open(strGpkgFileFullPath.c_str(), &pDB) != SQLITE_OK)
	{
		return false;
	}

	// 栅格分析就绪型数据包存储元数据表，从元数据表中读取元数据信息
	// SQL查询语句，元数据表"metadata"
	char szSQL[500] = { 0 };

	sprintf(szSQL, "select * from metadata");

	sqlite3_stmt* stmt;

	// 准备SQL语句
	if (sqlite3_prepare_v2(pDB, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
	{
		sqlite3_close(pDB);
		return false;
	}

	int rc = 0;
	// 执行查询并处理结果
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {

		// 分析就绪数据标识
		info.vIdentifys.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));

		// 分析就绪数据名称
		info.vNames.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));

		// 数据级别
		info.vDataLevels.push_back(sqlite3_column_int(stmt, 2));

		// 数据类型
		info.vDataTypes.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));

		SE_DRect dRect;

		// 左边界经度
		dRect.dleft = sqlite3_column_double(stmt, 4);
		
		// 上边界经度
		dRect.dtop = sqlite3_column_double(stmt, 5);

		// 右边界经度
		dRect.dright = sqlite3_column_double(stmt, 6);

		// 下边界经度
		dRect.dbottom = sqlite3_column_double(stmt, 7);

		info.vdRects.push_back(dRect);

		// 描述信息
		info.vDetails.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)));

		// 支撑的任务
		info.vTasks.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)));

		// 时相信息
		info.vTemporals.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)));

	}

	// 清理
	sqlite3_finalize(stmt);

	sqlite3_close(pDB);


	return true;
}


bool SE_ManageARDPackageDlg::DeleteVectorLayerByARDIdentify(string strGpkgFileFullName, string strARDIdentify)
{
	// 注册所有 GDAL/OGR 驱动
	GDALAllRegister();

	// 以可写模式打开数据源
	GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strGpkgFileFullName.c_str(), GDAL_OF_UPDATE, NULL, NULL, NULL);
	if (poDS == nullptr) {
		std::cerr << "[Error] Failed to open GeoPackage: " << strGpkgFileFullName << std::endl;
		return false;
	}

	int layerCount = poDS->GetLayerCount();
	bool bDeletedAny = false;

	for (int i = 0; i < layerCount; i++)
	{
		OGRLayer* poLayer = poDS->GetLayer(i);
		if (poLayer == nullptr)
			continue;

		// 判断图层名中是否包含指定标识
		const char* layerName = poLayer->GetName();
		if (layerName && strstr(layerName, strARDIdentify.c_str()) != nullptr)
		{
			// 找到匹配的图层，尝试删除
			if (poDS->DeleteLayer(i) == OGRERR_NONE)
			{
				//std::cout << "[Info] Deleted layer: " << layerName << std::endl;
				bDeletedAny = true;
				// 注意：删除一个图层后，后续图层索引会变化，处理方式有多种：
				// 这里简单做法是 i-- 并重置layerCount
				i--;
				layerCount = poDS->GetLayerCount();
			}
			else
			{
				// 记录日志
				continue;
				//std::cerr << "[Error] Failed to delete layer: " << layerName << std::endl;
			}
		}
	}

	// 更新元数据表
	if (!DeleteFeaturesByAttribute(strGpkgFileFullName,
		"metadata",
		"分析就绪型数据标识",
		strARDIdentify))
	{
		GDALClose(poDS);

		// 记录日志
		return false;
	}


	GDALClose(poDS);
	return true;
}


bool SE_ManageARDPackageDlg::DeleteRasterByARDIdentify(string strGpkgFileFullName, string strARDIdentify)
{
	sqlite3* db;
	char* errMsg = nullptr;
	std::string sql;

	// 打开数据库
	if (sqlite3_open(strGpkgFileFullName.c_str(), &db) != SQLITE_OK) {
		//std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
		return false;
	}

	// 开始事务
	if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
		std::cerr << "开始事务失败: " << errMsg << std::endl;
		sqlite3_free(errMsg);
		sqlite3_close(db);
		return false;
	}

	string strTableName = "RASTER_" + strARDIdentify;

	// 构造SQL语句
	sql = "DROP TABLE IF EXISTS " + strTableName + ";";

	// 执行SQL语句
	if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
		//std::cerr << "SQL错误: " << errMsg << std::endl;
		sqlite3_free(errMsg);

		// 回滚事务
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &errMsg);
		if (errMsg) {
			//std::cerr << "回滚事务失败: " << errMsg << std::endl;
			sqlite3_free(errMsg);
		}

		return false;
	}
	else {

		// 更新metadata元数据表
		if (!DeleteRowsFromMetadata(db, strARDIdentify))
		{
			// 记录日志，更新元数据表失败
			return false;
		}


		// 提交事务
		if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
			//std::cerr << "提交事务失败: " << errMsg << std::endl;
			sqlite3_free(errMsg);
		}
		else {
			//std::cout << "表 " << strTableName << " 删除成功。" << std::endl;
		}
	}

	// 关闭数据库
	sqlite3_close(db);

	return true;
}



void SE_ManageARDPackageDlg::on_lineEdit_InputDataPath_TextChanged(const QString& qstr)
{
	ui.treeWidget_ARD_Package->clear();
}


bool SE_ManageARDPackageDlg::DeleteRowsFromMetadata(sqlite3* db, const std::string& condition) 
{

	char* errMsg = 0;

	// 创建DELETE语句
	// "DELETE FROM metadata WHERE 分析就绪型数据标识 = '01_010002_00'"
	std::string sql = "DELETE FROM metadata WHERE 分析就绪型数据标识 = '" + condition + "'";

	// 执行DELETE语句
	if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
		//std::cerr << "SQL错误: " << errMsg << std::endl;
		sqlite3_free(errMsg);

		return false;
	}

	if (errMsg)
	{
		sqlite3_free(errMsg);
	}

	return true;

}



/**
  * 函数：DeleteFeaturesByAttribute
  * 说明：
  *   在指定的gpkg文件(datasourcePath)中，
  *   对名为 layerName 的表(如 "metadata")进行删除操作：
  *   删除所有满足 attributeName = attributeValue 的记录。
  *
  * 参数：
  *   @datasourcePath: GPKG 文件完整路径
  *   @layerName:      表名称（测试时可为 "metadata"）
  *   @attributeName:  表中字段名
  *   @attributeValue: 需要匹配的字段值
  *
  * 返回：
  *   @bool：删除操作成功返回 true，否则返回 false
  */
bool SE_ManageARDPackageDlg::DeleteFeaturesByAttribute(
	const std::string& datasourcePath,
	const std::string& layerName,
	const std::string& attributeName,
	const std::string& attributeValue)
{
	// 1) 注册所有 GDAL/OGR 驱动
	GDALAllRegister();

	// 2) 以可写 (GDAL_OF_UPDATE) 方式打开指定的 gpkg 文件
	GDALDataset* poDS = static_cast<GDALDataset*>(
		GDALOpenEx(datasourcePath.c_str(), GDAL_OF_UPDATE, nullptr, nullptr, nullptr)
		);
	if (poDS == nullptr) {
		std::cerr << "[Error] 无法打开数据源: " << datasourcePath << std::endl;
		return false;
	}

	// 3) 构建 DELETE 语句：
	//    使用双引号括起列名以处理包含特殊字符或中文的情况
	//    "DELETE FROM metadata WHERE \"分析就绪型数据标识\" = '02_140000_02'";
	std::string sql =
		"DELETE FROM \"" + layerName +
		"\" WHERE \"" + attributeName + "\" = '" + attributeValue + "'";

	// 输出调试信息（可选）
	std::cout << "执行的SQL语句: " << sql << std::endl;

	// 4) 使用 ExecuteSQL 执行删除
	OGRLayer* poResultLayer = poDS->ExecuteSQL(sql.c_str(), nullptr, nullptr);

	// 对于非 SELECT 语句，poResultLayer 通常为空，但依然需要释放
	poDS->ReleaseResultSet(poResultLayer);

	// 5) 关闭数据源，保证修改写回磁盘
	GDALClose(poDS);
	return true;
}


#pragma region  "检查文件或文件夹是否存在"

bool SE_ManageARDPackageDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion