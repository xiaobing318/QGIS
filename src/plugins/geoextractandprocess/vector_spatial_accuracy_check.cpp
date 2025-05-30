#include "vector_spatial_accuracy_check.h"

#include "qgshelp.h"
#include "qgsmaplayer.h"
#include "qgsproject.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"
#include "qgsrasterlayer.h"
#include "qgsvectorlayer.h"

#include <QFileDialog>
#include <QMessageBox>
#include "qgsproject.h"
#include <qlabel.h>
#include <qlineedit.h>
#include <qlabel.h>
#include "qgsmapmouseevent.h"
#include "qgspointxy.h"
#include "qgsmapcanvasitem.h"
#include "cse_geoextractandprocess.h"

#include "qgsmessagelog.h"
#include "qgsmessagebar.h"

#define DEGREE_2_RADIAN		0.017453292519943
#define RADIAN_2_DEGREE		57.295779513082321
#define GEOEXTRACT_ZERO		1.0e-6
const double _CGCS2000_SEMIMAJORAXIS = 6378137;				// CGCS2000坐标系参考椭球长半轴，单位为米
const double _CGCS2000_SEMIMINORAXIS = 6356752.3141403561;	// CGCS2000坐标系参考椭球短半轴，单位为米

QgsVectorSpatialAccuracyCheckDlg::QgsVectorSpatialAccuracyCheckDlg(QWidget* parent, Qt::WindowFlags fl)
	: QMainWindow(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);
	this->showMaximized();

	// 清空表格控件内容
	ui.tableWidget_CheckPoint->clearContents();

	//可以选中单个
	ui.tableWidget_CheckPoint->setSelectionMode(QAbstractItemView::SingleSelection);

	// 程序运行目录
	QString curPath = QCoreApplication::applicationDirPath();

	// 添加检查矢量
	QString strIconAddCheckVectorPath = curPath + "/resource/geoextractandprocess/layer-vector-add.png";
	QIcon iconAddCheckVector;
	iconAddCheckVector.addFile(strIconAddCheckVectorPath);
	ui.action_AddCheckVector->setIcon(iconAddCheckVector);
	ui.toolBar->addAction(ui.action_AddCheckVector);

	// 添加参考影像
	QString strIconAddReferenceImage = curPath + "/resource/geoextractandprocess/layer-raster-add.png";
	QIcon iconAddReferenceImage;
	iconAddReferenceImage.addFile(strIconAddReferenceImage);
	ui.action_AddReferenceImage->setIcon(iconAddReferenceImage);
	ui.toolBar->addAction(ui.action_AddReferenceImage);

	// 添加参考矢量
	QString strIconAddReferenceVector = curPath + "/resource/geoextractandprocess/layer-vector-add.png";
	QIcon iconAddReferenceVector;
	iconAddReferenceVector.addFile(strIconAddReferenceVector);
	ui.action_AddReferenceVector->setIcon(iconAddReferenceVector);
	ui.toolBar->addAction(ui.action_AddReferenceVector);

	// 增加检查点
	QString strIconAddCheckPoint = curPath + "/resource/geoextractandprocess/add_point.png";
	QIcon iconAddCheckPoint;
	iconAddCheckPoint.addFile(strIconAddCheckPoint);
	ui.action_AddCheckPoint->setIcon(iconAddCheckPoint);
	ui.toolBar->addAction(ui.action_AddCheckPoint);

	// 删除检查点
	QString strIconDeleteCheckPoint = curPath + "/resource/geoextractandprocess/delete_point.png";
	QIcon iconDeleteCheckPoint;
	iconDeleteCheckPoint.addFile(strIconDeleteCheckPoint);
	ui.action_DeleteCheckPoint->setIcon(iconDeleteCheckPoint);
	ui.toolBar->addAction(ui.action_DeleteCheckPoint);

	// 删除所有检查点
	QString strIconDeleteAllCheckPoints = curPath + "/resource/geoextractandprocess/delete_allpoints.png";
	QIcon iconDeleteAllCheckPoints;
	iconDeleteAllCheckPoints.addFile(strIconDeleteAllCheckPoints);
	ui.action_DeleteAllCheckPoints->setIcon(iconDeleteAllCheckPoints);
	ui.toolBar->addAction(ui.action_DeleteAllCheckPoints);

	// 增加参考点
	QString strIconAddRefPoint = curPath + "/resource/geoextractandprocess/add_point.png";
	QIcon iconAddRefPoint;
	iconAddRefPoint.addFile(strIconAddRefPoint);
	ui.action_AddRefPoint->setIcon(iconAddRefPoint);
	ui.toolBar->addAction(ui.action_AddRefPoint);

	// 删除参考点
	QString strIconDeleteRefPoint = curPath + "/resource/geoextractandprocess/delete_point.png";
	QIcon iconDeleteRefPoint;
	iconDeleteRefPoint.addFile(strIconDeleteRefPoint);
	ui.action_DeleteRefPoint->setIcon(iconDeleteRefPoint);
	ui.toolBar->addAction(ui.action_DeleteRefPoint);

	// 删除所有参考点
	QString strIconDeleteAllReferPoints = curPath + "/resource/geoextractandprocess/delete_allpoints.png";
	QIcon iconDeleteAllReferPoints;
	iconDeleteAllReferPoints.addFile(strIconDeleteAllReferPoints);
	ui.action_DeleteAllReferPoints->setIcon(iconDeleteAllReferPoints);
	ui.toolBar->addAction(ui.action_DeleteAllReferPoints);

	// 放大
	QString strIconZoomInCheck = curPath + "/resource/geoextractandprocess/zoom-in.png";
	QIcon iconZoomInCheck;
	iconZoomInCheck.addFile(strIconZoomInCheck);
	ui.action_ZoomInCheck->setIcon(iconZoomInCheck);
	ui.toolBar->addAction(ui.action_ZoomInCheck);

	// 缩小
	QString strIconZoomOutCheck = curPath + "/resource/geoextractandprocess/zoom-out.png";
	QIcon iconZoomOutCheck;
	iconZoomOutCheck.addFile(strIconZoomOutCheck);
	ui.action_ZoomOutCheck->setIcon(iconZoomOutCheck);
	ui.toolBar->addAction(ui.action_ZoomOutCheck);

	// 漫游
	QString strIconPanCheck = curPath + "/resource/geoextractandprocess/pan.png";
	QIcon iconPanCheck;
	iconPanCheck.addFile(strIconPanCheck);
	ui.action_PanCheck->setIcon(iconPanCheck);
	ui.toolBar->addAction(ui.action_PanCheck);

	// 放大
	QString strIconZoomInRefer = curPath + "/resource/geoextractandprocess/zoom-in.png";
	QIcon iconZoomInRefer;
	iconZoomInRefer.addFile(strIconZoomInRefer);
	ui.action_ZoomInRefer->setIcon(iconZoomInRefer);
	ui.toolBar->addAction(ui.action_ZoomInRefer);

	// 缩小
	QString strIconZoomOutRefer = curPath + "/resource/geoextractandprocess/zoom-out.png";
	QIcon iconZoomOutRefer;
	iconZoomOutRefer.addFile(strIconZoomOutRefer);
	ui.action_ZoomOutRefer->setIcon(iconZoomOutRefer);
	ui.toolBar->addAction(ui.action_ZoomOutRefer);

	// 漫游
	QString strIconPanRefer = curPath + "/resource/geoextractandprocess/pan.png";
	QIcon iconPanRefer;
	iconPanRefer.addFile(strIconPanRefer);
	ui.action_PanRefer->setIcon(iconPanRefer);
	ui.toolBar->addAction(ui.action_PanRefer);

	// 空间精度检查
	QString strCheckAccuracy = curPath + "/resource/geoextractandprocess/calculate_m.png";
	QIcon iconCheckAccuracy;
	iconCheckAccuracy.addFile(strCheckAccuracy);
	ui.action_CheckAccuracy->setIcon(iconCheckAccuracy);
	ui.toolBar->addAction(ui.action_CheckAccuracy);

	// 剔除粗差重新计算
	QString strRejectCoarseDifferences = curPath + "/resource/geoextractandprocess/reject_coarse.png";
	QIcon iconRejectCoarseDifferences;
	iconRejectCoarseDifferences.addFile(strRejectCoarseDifferences);
	ui.action_RejectCoarseDifferences->setIcon(iconRejectCoarseDifferences);
	ui.toolBar->addAction(ui.action_RejectCoarseDifferences);

	// 生成精度检查报告
	QString strCreateReport = curPath + "/resource/geoextractandprocess/create_report.png";
	QIcon iconCreateReport;
	iconCreateReport.addFile(strCreateReport);
	ui.action_CreateReport->setIcon(iconCreateReport);
	ui.toolBar->addAction(ui.action_CreateReport);

	// 退出
	QString strExit = curPath + "/resource/geoextractandprocess/quit.png";
	QIcon iconExit;
	iconExit.addFile(strExit);
	ui.action_Exit->setIcon(iconExit);
	ui.toolBar->addAction(ui.action_Exit);

	// 新建检查地图画布
	mapCanvas_Check = new QgsMapCanvas();
	ui.tabWidget_CheckVector->addTab(mapCanvas_Check, tr("待检查矢量数据"));
	mapCanvas_Check->enableAntiAliasing(true);
	mapCanvas_Check->setCanvasColor(QColor(255, 255, 255));
	mapCanvas_Check->setVisible(true);

	// 新建参考地图画布
	mapCanvas_Refer = new QgsMapCanvas();
	ui.tabWidget_RefData->addTab(mapCanvas_Refer, tr("参考数据"));
	mapCanvas_Refer->enableAntiAliasing(true);
	mapCanvas_Refer->setCanvasColor(QColor(255, 255, 255));
	mapCanvas_Refer->setVisible(true);

	// 信号-槽响应函数
	connect(ui.action_AddCheckVector, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddCheckVector_slot);
	connect(ui.action_AddReferenceImage, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddReferenceImage_slot);
	connect(ui.action_AddReferenceVector, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddReferenceVector_slot);

	connect(ui.action_Exit, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_Exit_slot);

	connect(ui.action_AddCheckPoint, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddCheckPoint_slot);
	connect(ui.action_DeleteCheckPoint, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_DeleteCheckPoint_slot);
	connect(ui.action_DeleteAllCheckPoints, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_DeleteAllCheckPoints_slot);

	connect(ui.action_AddRefPoint, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddReferPoint_slot);
	connect(ui.action_DeleteRefPoint, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_DeleteReferPoint_slot);
	connect(ui.action_DeleteAllReferPoints, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_DeleteAllReferPoints_slot);

	connect(ui.action_CheckAccuracy, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_CheckAccuracy_slot);
	connect(ui.action_RejectCoarseDifferences, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::action_RejectCoarseDifferences_slot);

	connect(ui.action_ZoomInCheck, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::ZoomInCheckImage);
	connect(ui.action_ZoomOutCheck, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::ZoomOutCheckImage);
	connect(ui.action_PanCheck, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::PanCheckImage);

	connect(ui.action_ZoomInRefer, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::ZoomInReferImage);
	connect(ui.action_ZoomOutRefer, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::ZoomOutReferImage);
	connect(ui.action_PanRefer, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::PanReferImage);

	connect(ui.action_CreateReport, &QAction::triggered, this, &QgsVectorSpatialAccuracyCheckDlg::CreateReport);

	//------------------------------------------------//
	// 检查图放大工具
	m_ZoomInTool_Check = new QgsMapToolZoom(mapCanvas_Check, false);
	m_ZoomInTool_Check->setAction(ui.action_ZoomInCheck);

	// 检查图缩小工具
	m_ZoomOutTool_Check = new QgsMapToolZoom(mapCanvas_Check, true);
	m_ZoomOutTool_Check->setAction(ui.action_ZoomOutCheck);

	// 检查图漫游工具
	m_PanTool_Check = new QgsMapToolPan(mapCanvas_Check);
	m_PanTool_Check->setAction(ui.action_PanCheck);

	// 参考图放大工具
	m_ZoomInTool_Refer = new QgsMapToolZoom(mapCanvas_Refer, false);
	m_ZoomInTool_Refer->setAction(ui.action_ZoomInRefer);

	// 参考图缩小工具
	m_ZoomOutTool_Refer = new QgsMapToolZoom(mapCanvas_Refer, true);
	m_ZoomOutTool_Refer->setAction(ui.action_ZoomOutRefer);

	// 参考图漫游工具
	m_PanTool_Refer = new QgsMapToolPan(mapCanvas_Refer);
	m_PanTool_Refer->setAction(ui.action_PanRefer);

	/*检查图上鼠标左键关联*/
	m_EmitPoint_Check = new QgsMapToolEmitPoint(mapCanvas_Check);
	m_EmitPoint_Check->setAction(ui.action_AddCheckPoint);

	connect(m_EmitPoint_Check, &QgsMapToolEmitPoint::canvasClicked,
		this, &QgsVectorSpatialAccuracyCheckDlg::GetMouseClickedXY_Check);

	/*参考图上鼠标左键关联*/
	m_EmitPoint_Refer = new QgsMapToolEmitPoint(mapCanvas_Refer);
	m_EmitPoint_Refer->setAction(ui.action_AddRefPoint);

	connect(m_EmitPoint_Refer, &QgsMapToolEmitPoint::canvasClicked,
		this, &QgsVectorSpatialAccuracyCheckDlg::GetMouseClickedXY_Refer);

	//----------------------------------------------//

	// 关联按钮
	connect(ui.pushButton_AddCheckVector, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddCheckVector_slot);
	connect(ui.pushButton_ZoomInCheck, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::ZoomInCheckImage);
	connect(ui.pushButton_ZoomOutCheck, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::ZoomOutCheckImage);
	connect(ui.pushButton_PanCheck, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::PanCheckImage);
	connect(ui.pushButton_AddCheckPoint, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddCheckPoint_slot);

	connect(ui.pushButton_DeleteCheckPoint, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_DeleteCheckPoint_slot);
	connect(ui.pushButton_DeleteAllCheckPoints, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_DeleteAllCheckPoints_slot);
	connect(ui.pushButton_AddReferenceImage, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddReferenceImage_slot);
	connect(ui.pushButton_AddReferenceVector, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddReferenceVector_slot);
	connect(ui.pushButton_ZoomInRefer, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::ZoomInReferImage);

	connect(ui.pushButton_ZoomOutRefer, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::ZoomOutReferImage);
	connect(ui.pushButton_PanRefer, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::PanReferImage);
	connect(ui.pushButton_AddRefPoint, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_AddReferPoint_slot);
	connect(ui.pushButton_DeleteRefPoint, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_DeleteReferPoint_slot);
	connect(ui.pushButton_DeleteAllReferPoints, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_DeleteAllReferPoints_slot);

	connect(ui.pushButton_CheckAccuracy, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_CheckAccuracy_slot);
	connect(ui.pushButton_RejectCoarseDifferences, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::action_RejectCoarseDifferences_slot);

	connect(ui.pushButton_CreateReport, &QPushButton::clicked, this, &QgsVectorSpatialAccuracyCheckDlg::CreateReport);
	//----------------------------------------------//
	m_vCheckPoint.clear();
	m_vReferPoint.clear();

	m_vCheckPointMarker.clear();
	m_vReferPointMarker.clear();
}

QgsVectorSpatialAccuracyCheckDlg::~QgsVectorSpatialAccuracyCheckDlg()
{
	delete m_ZoomInTool_Check;
	delete m_ZoomOutTool_Check;
	delete m_PanTool_Check;
	delete m_ZoomInTool_Refer;
	delete m_ZoomOutTool_Refer;
	delete m_PanTool_Refer;
	delete m_EmitPoint_Check;
	delete mapCanvas_Check;
	delete mapCanvas_Refer;

	for (int i = 0; i < m_vCheckPointMarker.size(); i++)
	{
		if (m_vCheckPointMarker[i])
		{
			delete m_vCheckPointMarker[i];
		}
	}

	for (int i = 0; i < m_vReferPointMarker.size(); i++)
	{
		if (m_vReferPointMarker[i])
		{
			delete m_vReferPointMarker[i];
		}
	}
}

// 加载待检查矢量
void QgsVectorSpatialAccuracyCheckDlg::action_AddCheckVector_slot()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString fileName = QFileDialog::getOpenFileName(this, tr("加载待检查矢量数据"), curPath, "*.shp *.SHP");
	if (fileName.isEmpty()) //如果文件未选择则返回
	{
		QgsMessageLog::logMessage(tr("生成精度检查报告成功！"), tr("矢量数据空间精度检查"), Qgis::Warning);
		return;
	}

	QFileInfo fileinfo(fileName);

	// 获取数据名称
	QString basename = fileinfo.completeBaseName();

	QgsVectorLayer* vecLayer = new QgsVectorLayer(fileName, basename, "ogr");

	if (!vecLayer->isValid())
	{
		QMessageBox::critical(this, "error", QString(tr("图层不合法: \n")) + fileName);
		return;
	}

	QgsProject::instance()->addMapLayer(vecLayer);

	// 设置显示范围
	mapCanvas_Check->setExtent(vecLayer->extent());

	// 添加到画布图集中
	mapCanvasLayerSet_Check.append(vecLayer);

	mapCanvas_Check->setLayers(mapCanvasLayerSet_Check);

	// 设置可见性
	mapCanvas_Check->setVisible(true);

	// 设置是否冻结对图层的操作
	mapCanvas_Check->freeze(false);

	// 刷新图层
	mapCanvas_Check->refresh();
}

// 加载参考影像数据
void QgsVectorSpatialAccuracyCheckDlg::action_AddReferenceImage_slot()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString filename = QFileDialog::getOpenFileName(this,
		tr("加载参考影像数据"),
		curPath,
		"image(*.jpg *.png *.bmp *.tif)");

	// 如果没有选中数据，则返回
	if (filename.isEmpty())
	{
		return;
	}

	QFileInfo fileinfo(filename);

	// 获取数据名称
	QString basename = fileinfo.completeBaseName();

	// 根据文件存放路径，栅格的名称创建一个QgsRasterLayer类
	QgsRasterLayer* rasterLayer = new QgsRasterLayer(filename, basename);
	if (!rasterLayer || !rasterLayer->isValid())
	{
		QMessageBox::critical(this, "error", QString(tr("图层不合法: \n")) + filename);
		return;
	}

	QgsProject::instance()->addMapLayer(rasterLayer);

	// 设置显示范围
	mapCanvas_Refer->setExtent(rasterLayer->extent());

	// 添加到画布图集中
	mapCanvasLayerSet_Refer.append(rasterLayer);

	mapCanvas_Refer->setLayers(mapCanvasLayerSet_Refer);

	// 设置可见性
	mapCanvas_Refer->setVisible(true);

	// 设置是否冻结对图层的操作
	mapCanvas_Refer->freeze(false);

	// 刷新图层
	mapCanvas_Refer->refresh();
}

// 加载参考矢量数据
void QgsVectorSpatialAccuracyCheckDlg::action_AddReferenceVector_slot()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString fileName = QFileDialog::getOpenFileName(this, tr("加载参考矢量数据"), curPath, "*.shp *.SHP");
	if (fileName.isEmpty()) //如果文件未选择则返回
	{
		return;
	}

	QFileInfo fileinfo(fileName);
	// 获取数据名称
	QString basename = fileinfo.completeBaseName();

	QgsVectorLayer* vecLayer = new QgsVectorLayer(fileName, basename, "ogr");

	if (!vecLayer->isValid())
	{
		QMessageBox::critical(this, "error", QString(tr("图层不合法: \n")) + fileName);
		return;
	}

	QgsProject::instance()->addMapLayer(vecLayer);

	// 设置显示范围
	mapCanvas_Refer->setExtent(vecLayer->extent());

	// 添加到画布图集中
	mapCanvasLayerSet_Refer.append(vecLayer);

	mapCanvas_Refer->setLayers(mapCanvasLayerSet_Refer);

	// 设置可见性
	mapCanvas_Refer->setVisible(true);

	// 设置是否冻结对图层的操作
	mapCanvas_Refer->freeze(false);

	// 刷新图层
	mapCanvas_Refer->refresh();
}

// 退出按钮
void QgsVectorSpatialAccuracyCheckDlg::action_Exit_slot()
{
	this->close();
}

void QgsVectorSpatialAccuracyCheckDlg::action_AddCheckPoint_slot()
{
	mapCanvas_Check->setMapTool(m_EmitPoint_Check);
}

// 删除检查点
void QgsVectorSpatialAccuracyCheckDlg::action_DeleteCheckPoint_slot()
{
	// 获取表格控件中的某一行，然后删除对应记录
	int iCurrentRow = ui.tableWidget_CheckPoint->currentRow();
	if (iCurrentRow < 0 || iCurrentRow >= ui.tableWidget_CheckPoint->rowCount())
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("请选择要删除的检查点所在行！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	// 获取对应记录的检查点坐标，并删除
	double dCheckPointX = ui.tableWidget_CheckPoint->item(iCurrentRow, 1)->text().toDouble();
	double dCheckPointY = ui.tableWidget_CheckPoint->item(iCurrentRow, 2)->text().toDouble();
	for (int i = 0; i < m_vCheckPointMarker.size(); i++)
	{
		if (fabs(m_vCheckPointMarker[i]->center().x() - dCheckPointX) < GEOEXTRACT_ZERO
			&& fabs(m_vCheckPointMarker[i]->center().y() - dCheckPointY) < GEOEXTRACT_ZERO)
		{
			delete m_vCheckPointMarker[i];
		}
	}

	ui.tableWidget_CheckPoint->removeRow(iCurrentRow);
}

// 删除全部检查点
void QgsVectorSpatialAccuracyCheckDlg::action_DeleteAllCheckPoints_slot()
{
	for (int row = ui.tableWidget_CheckPoint->rowCount() - 1; row >= 0; row--)
	{
		ui.tableWidget_CheckPoint->removeRow(row);
	}
	m_vCheckPoint.clear();
	m_vReferPoint.clear();

	for (int i = 0; i < m_vCheckPointMarker.size(); i++)
	{
		if (m_vCheckPointMarker[i])
		{
			delete m_vCheckPointMarker[i];
		}
	}
	m_vCheckPointMarker.clear();
}

// 添加参考点
void QgsVectorSpatialAccuracyCheckDlg::action_AddReferPoint_slot()
{
	mapCanvas_Refer->setMapTool(m_EmitPoint_Refer);
}

// 删除参考点
void QgsVectorSpatialAccuracyCheckDlg::action_DeleteReferPoint_slot()
{
	// 获取表格控件中的某一行，然后删除对应记录
	int iCurrentRow = ui.tableWidget_CheckPoint->currentRow();
	if (iCurrentRow < 0 || iCurrentRow >= ui.tableWidget_CheckPoint->rowCount())
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("请选择要删除的参考点所在行！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	// 获取对应记录的参考点坐标，并删除
	double dReferPointX = ui.tableWidget_CheckPoint->item(iCurrentRow, 3)->text().toDouble();
	double dReferPointY = ui.tableWidget_CheckPoint->item(iCurrentRow, 4)->text().toDouble();
	for (int i = 0; i < m_vReferPointMarker.size(); i++)
	{
		if (fabs(m_vReferPointMarker[i]->center().x() - dReferPointX) < GEOEXTRACT_ZERO
			&& fabs(m_vReferPointMarker[i]->center().y() - dReferPointY) < GEOEXTRACT_ZERO)
		{
			delete m_vReferPointMarker[i];
		}
	}

	ui.tableWidget_CheckPoint->removeRow(iCurrentRow);
}

// 删除全部参考点
void QgsVectorSpatialAccuracyCheckDlg::action_DeleteAllReferPoints_slot()
{
	for (int row = ui.tableWidget_CheckPoint->rowCount() - 1; row >= 0; row--)
	{
		ui.tableWidget_CheckPoint->removeRow(row);
	}
	m_vCheckPoint.clear();
	m_vReferPoint.clear();

	for (int i = 0; i < m_vReferPointMarker.size(); i++)
	{
		if (m_vReferPointMarker[i])
		{
			delete m_vReferPointMarker[i];
		}
	}
	m_vReferPointMarker.clear();
}

// 空间精度检查
void QgsVectorSpatialAccuracyCheckDlg::action_CheckAccuracy_slot()
{
	if (mapCanvasLayerSet_Check.size() == 0)
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("请加载待检查矢量数据！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	if (mapCanvasLayerSet_Refer.size() == 0)
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("请加载参考影像或矢量数据！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	// 如果检查图和参考图空间参考不一致，进行提示
	// 获取检查图空间参考系
	QgsMapLayer* pCheckLayer = mapCanvasLayerSet_Check.back();
	QgsCoordinateReferenceSystem checkLayerCrs = pCheckLayer->crs();

	// 获取参考图空间参考系
	QgsMapLayer* pRefLayer = mapCanvasLayerSet_Refer.back();
	QgsCoordinateReferenceSystem refLayerCrs = pRefLayer->crs();

	if (checkLayerCrs != refLayerCrs)
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("检查图和参考图空间参考不一致，请重新加载检查图或参考图！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	// 根据检查点和参考点
	if (m_vCheckPoint.size() == 0 || m_vReferPoint.size() == 0)
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("请添加检查点和参考点坐标！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	// 根据检查点和参考点
	if (m_vCheckPoint.size() != m_vReferPoint.size())
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("检查点数和参考点数不一致，请重新设置检查点和参考点！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	// 计算每对点之间的距离
	vector<double> vDistance;
	vDistance.clear();

	for (int i = 0; i < m_vCheckPoint.size(); i++)
	{
		double dDistance = CalDistanceOfTwoPoints(m_vCheckPoint[i], m_vReferPoint[i], _CGCS2000_SEMIMAJORAXIS, _CGCS2000_SEMIMINORAXIS);
		vDistance.push_back(dDistance);

		// 参考点坐标
		QString strDistance;
		strDistance.sprintf("%.6f", dDistance);
		ui.tableWidget_CheckPoint->setItem(i, 5, new QTableWidgetItem(strDistance));
	}

	// 从表格控件中获取检查点、参考点坐标
	int iRowCount = ui.tableWidget_CheckPoint->rowCount();
	if (iRowCount == 0)
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("请添加检查点和参考点！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	// 表格中的检查点坐标
	vector<SE_DPoint> vCheckPointsInTable;
	vCheckPointsInTable.clear();

	// 表格中的参考点坐标
	vector<SE_DPoint> vReferPointsInTable;
	vReferPointsInTable.clear();

	for (int i = 0; i < iRowCount; i++)
	{
		SE_DPoint dCheckPoint;
		SE_DPoint dReferPoint;
		dCheckPoint.dx = ui.tableWidget_CheckPoint->item(i, 1)->text().toDouble();
		dCheckPoint.dy = ui.tableWidget_CheckPoint->item(i, 2)->text().toDouble();

		dReferPoint.dx = ui.tableWidget_CheckPoint->item(i, 3)->text().toDouble();
		dReferPoint.dy = ui.tableWidget_CheckPoint->item(i, 4)->text().toDouble();

		vCheckPointsInTable.push_back(dCheckPoint);
		vReferPointsInTable.push_back(dReferPoint);
	}

	m_vTableCheckPoint.clear();
	m_vTableReferPoint.clear();

	m_vTableCheckPoint = vCheckPointsInTable;
	m_vTableReferPoint = vReferPointsInTable;

	// 方差
	double dVariance = 0;
	int iResult = CSE_GeoExtractAndProcess::CalVariance(vCheckPointsInTable,
		vReferPointsInTable,
		dVariance);
	if (iResult != 0)
	{
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("中误差计算失败！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}
	else
	{
		double dMeanSquareError = sqrt(dVariance);
		m_dMeanSquareError = dMeanSquareError;
		QString strMeanSquareError;
		strMeanSquareError.sprintf("±%.6f", dMeanSquareError);
		ui.lineEdit_MeanSquareError->setText(strMeanSquareError);

		//消息工具条方式，在窗口顶端显示//
		QgsMessageBar* mMessageBar = new QgsMessageBar(centralWidget());
		mMessageBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
		static_cast<QGridLayout*>(centralWidget()->layout())->addWidget(mMessageBar, 0, 0, 1, 1, Qt::AlignTop);
		mMessageBar->pushMessage(tr("测绘地理数据预处理"),
			tr("中误差计算完毕"),
			Qgis::MessageLevel::Success, 0);

		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("测绘地理数据预处理"));
		msgBox1.setText(tr("中误差计算完毕！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}
}

// 剔除粗差重新计算
void QgsVectorSpatialAccuracyCheckDlg::action_RejectCoarseDifferences_slot()
{
}

void QgsVectorSpatialAccuracyCheckDlg::ZoomInCheckImage()
{
	mapCanvas_Check->setMapTool(m_ZoomInTool_Check);
}

void QgsVectorSpatialAccuracyCheckDlg::ZoomOutCheckImage()
{
	mapCanvas_Check->setMapTool(m_ZoomOutTool_Check);
}

void QgsVectorSpatialAccuracyCheckDlg::PanCheckImage()
{
	mapCanvas_Check->setMapTool(m_PanTool_Check);
}

void QgsVectorSpatialAccuracyCheckDlg::ZoomInReferImage()
{
	mapCanvas_Refer->setMapTool(m_ZoomInTool_Refer);
}

void QgsVectorSpatialAccuracyCheckDlg::ZoomOutReferImage()
{
	mapCanvas_Refer->setMapTool(m_ZoomOutTool_Refer);
}

void QgsVectorSpatialAccuracyCheckDlg::PanReferImage()
{
	mapCanvas_Refer->setMapTool(m_PanTool_Refer);
}

void QgsVectorSpatialAccuracyCheckDlg::GetMouseClickedXY_Check(const QgsPointXY& xy, Qt::MouseButton button)
{
	QgsVertexMarker* pMarker = NULL;
	// 单击左键
	if (Qt::LeftButton == button)
	{
		const QgsPointXY mapCoordPoint = xy;

		SE_DPoint dPoint(mapCoordPoint.x(), mapCoordPoint.y());
		m_vCheckPoint.push_back(dPoint);

		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = ui.tableWidget_CheckPoint->rowCount();

		// 如果表格行数小于点个数
		if (iRowCount < m_vCheckPoint.size())
		{
			//增加一行
			ui.tableWidget_CheckPoint->insertRow(iRowCount);
			// 序号从1开始
			QString strSeqID;
			strSeqID.sprintf("%d", m_vCheckPoint.size());

			// 插入序号
			ui.tableWidget_CheckPoint->setItem(iRowCount, 0, new QTableWidgetItem(strSeqID));

			// 参考点坐标
			QString strX;
			strX.sprintf("%.8f", dPoint.dx);
			ui.tableWidget_CheckPoint->setItem(iRowCount, 1, new QTableWidgetItem(strX));

			// 检查点坐标
			QString strY;
			strY.sprintf("%.8f", dPoint.dy);
			ui.tableWidget_CheckPoint->setItem(iRowCount, 2, new QTableWidgetItem(strY));
		}
		// 如果表格行数等于点个数
		else if (iRowCount >= m_vCheckPoint.size())
		{
			// 序号从1开始
			QString strSeqID;
			strSeqID.sprintf("%d", m_vCheckPoint.size());

			// 插入序号
			ui.tableWidget_CheckPoint->setItem(m_vCheckPoint.size() - 1, 0, new QTableWidgetItem(strSeqID));

			// 检查点坐标
			QString strX;
			strX.sprintf("%.8f", dPoint.dx);
			ui.tableWidget_CheckPoint->setItem(m_vCheckPoint.size() - 1, 1, new QTableWidgetItem(strX));

			// 检查点坐标
			QString strY;
			strY.sprintf("%.8f", dPoint.dy);
			ui.tableWidget_CheckPoint->setItem(m_vCheckPoint.size() - 1, 2, new QTableWidgetItem(strY));
		}

		// 绘制检查点
		pMarker = new QgsVertexMarker(mapCanvas_Check);
		if (!pMarker)
		{
			return;
		}

		pMarker->setCenter(xy);
		pMarker->setColor(QColor(0, 255, 0));
		pMarker->setIconSize(12);
		pMarker->setIconType(QgsVertexMarker::ICON_CROSS);
		pMarker->setPenWidth(2);

		m_vCheckPointMarker.push_back(pMarker);
	}

	activateWindow();
	raise();
}

void QgsVectorSpatialAccuracyCheckDlg::GetMouseClickedXY_Refer(const QgsPointXY& xy, Qt::MouseButton button)
{
	QgsVertexMarker* pMarker = NULL;
	if (Qt::LeftButton == button)
	{
		const QgsPointXY mapCoordPoint = xy;

		SE_DPoint dPoint(mapCoordPoint.x(), mapCoordPoint.y());
		m_vReferPoint.push_back(dPoint);

		// 在表格中增加记录
		int iRowCount = 0;
		iRowCount = ui.tableWidget_CheckPoint->rowCount();

		// 如果表格行数小于点个数
		if (iRowCount < m_vReferPoint.size())
		{
			//增加一行
			ui.tableWidget_CheckPoint->insertRow(iRowCount);
			// 序号从1开始
			QString strSeqID;
			strSeqID.sprintf("%d", m_vReferPoint.size());

			// 插入序号
			ui.tableWidget_CheckPoint->setItem(iRowCount, 0, new QTableWidgetItem(strSeqID));

			// 参考点坐标
			QString strX;
			strX.sprintf("%.8f", dPoint.dx);
			ui.tableWidget_CheckPoint->setItem(iRowCount, 3, new QTableWidgetItem(strX));

			// 检查点坐标
			QString strY;
			strY.sprintf("%.8f", dPoint.dy);
			ui.tableWidget_CheckPoint->setItem(iRowCount, 4, new QTableWidgetItem(strY));
		}
		// 如果表格行数等于点个数
		else if (iRowCount >= m_vReferPoint.size())
		{
			// 序号从1开始
			QString strSeqID;
			strSeqID.sprintf("%d", m_vReferPoint.size());

			// 插入序号
			ui.tableWidget_CheckPoint->setItem(m_vReferPoint.size() - 1, 0, new QTableWidgetItem(strSeqID));

			// 检查点坐标
			QString strX;
			strX.sprintf("%.8f", dPoint.dx);
			ui.tableWidget_CheckPoint->setItem(m_vReferPoint.size() - 1, 3, new QTableWidgetItem(strX));

			// 检查点坐标
			QString strY;
			strY.sprintf("%.8f", dPoint.dy);
			ui.tableWidget_CheckPoint->setItem(m_vReferPoint.size() - 1, 4, new QTableWidgetItem(strY));
		}

		// 绘制参考点
		pMarker = new QgsVertexMarker(mapCanvas_Refer);
		if (!pMarker)
		{
			return;
		}

		pMarker->setCenter(xy);
		pMarker->setColor(QColor(0, 255, 0));
		pMarker->setIconSize(12);
		pMarker->setIconType(QgsVertexMarker::ICON_CROSS);
		pMarker->setPenWidth(2);

		m_vReferPointMarker.push_back(pMarker);
	}

	activateWindow();
	raise();
}

/* @brief 计算两点的大地线距离
*
* @param dPoint1:               点坐标1
* @param dPoint2:		        点坐标2
*
* @return 两点间的大地线距离
*/
double QgsVectorSpatialAccuracyCheckDlg::CalDistanceOfTwoPoints(SE_DPoint dPoint1, SE_DPoint dPoint2, double dSemiMajorAxis, double dSemiMinorAxis)
{
	double dDistance = 0;
	double x1 = dPoint1.dx;
	double y1 = dPoint1.dy;

	double x2 = dPoint2.dx;
	double y2 = dPoint2.dy;

	// 计算参考椭球体扁率
	double f = (dSemiMajorAxis - dSemiMinorAxis) / dSemiMajorAxis;
	double L = (x2 - x1) * DEGREE_2_RADIAN;
	double U1 = atan((1 - f) * tan(y1 * DEGREE_2_RADIAN));
	double U2 = atan((1 - f) * tan(y2 * DEGREE_2_RADIAN));

	double sinU1 = sin(U1);
	double cosU1 = cos(U1);
	double sinU2 = sin(U2);
	double cosU2 = cos(U2);

	double lam = L;
	int i = 0;
	double cosSqlAlpha = 0;
	double sinSigma = 0;
	double cosSigma = 0;
	double cos2SigmaM = 0;
	double sigma = 0;
	for (i = 0; i < 100; i++)
	{
		double sinLam = sin(lam);
		double cosLam = cos(lam);
		sinSigma = sqrt((cosU2 * sinLam) * (cosU2 * sinLam) +
			(cosU1 * sinU2 - sinU1 * cosU2 * cosLam) * (cosU1 * sinU2 - sinU1 * cosU2 * cosLam));

		if (fabs(sinSigma) < 1e-6)
		{
			// 重合点
			dDistance = 0;
			return dDistance;
		}

		cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLam;
		sigma = atan2(sinSigma, cosSigma);
		double sinAlpha = cosU1 * cosU2 * sinLam / sinSigma;
		cosSqlAlpha = 1 - sinAlpha * sinAlpha;
		cos2SigmaM = cosSigma - 2 * sinU1 * sinU2 / cosSqlAlpha;

		double C = f / 16 * cosSqlAlpha * (4 + f * (4 - 3 * cosSqlAlpha));
		double LP = lam;

		lam = L + (1 - C) * f * sinAlpha * \
			(sigma + C * sinSigma * (cos2SigmaM + C * cosSigma *
				(-1 + 2 * cos2SigmaM * cos2SigmaM)));

		if (fabs(lam - LP) <= 1e-12)
		{
			break;
		}
	}

	double uSq = cosSqlAlpha * (dSemiMajorAxis * dSemiMajorAxis - dSemiMinorAxis * dSemiMinorAxis) / (dSemiMinorAxis * dSemiMinorAxis);
	double A = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
	double B = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));
	double deltaSigma = B * sinSigma * (cos2SigmaM + B / 4 *
		(cosSigma * (-1 + 2 * cos2SigmaM * cos2SigmaM) -
			B / 6 * cos2SigmaM * (-3 + 4 * sinSigma * sinSigma) *
			(-3 + 4 * cos2SigmaM * cos2SigmaM)));
	double s = dSemiMinorAxis * A * (sigma - deltaSigma);
	dDistance = s;
	return dDistance;
}

void QgsVectorSpatialAccuracyCheckDlg::CreateReport()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = "生成矢量空间精度检查报告";
	QString filter = "txt 文件(*.txt)";
	QString qstrFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!qstrFileName.isEmpty())
	{
		ui.lineEdit_ReportPath->setText(qstrFileName);
		string strFileName = qstrFileName.toLocal8Bit();
		FILE* fp = fopen(strFileName.c_str(), "w");
		if (!fp)
		{
			// 增加日志信息
			QgsMessageLog::logMessage(tr("打开报告文件失败"), tr("矢量数据空间精度检查"), Qgis::Critical);
		}

		fprintf(fp, "\n-----------------------------------------\n");
		fprintf(fp, "           矢量数据空间精度检查报告           ");
		fprintf(fp, "\n-----------------------------------------\n");

		fprintf(fp, "\n        检查点和参考点信息        \n");
		fprintf(fp, "\n检查点个数：%d，参考点个数：%d\n", m_vTableCheckPoint.size(), m_vTableReferPoint.size());
		fprintf(fp, "\n序号\t检查点X\t\t检查点Y\t\t参考点X\t\t参考点Y\t\t距离\t\n");

		// 粗差点个数
		int iCoarseCount = 0;
		for (int i = 0; i < m_vTableCheckPoint.size(); i++)
		{
			double dDistance = CalDistanceOfTwoPoints(m_vTableCheckPoint[i], m_vTableReferPoint[i], _CGCS2000_SEMIMAJORAXIS, _CGCS2000_SEMIMINORAXIS);
			fprintf(fp, "%d\t%.8f\t%.8f\t%.8f\t%.8f\t\%.6f\n",
				i + 1,
				m_vTableCheckPoint[i].dx,
				m_vTableCheckPoint[i].dy,
				m_vTableReferPoint[i].dx,
				m_vTableReferPoint[i].dy,
				dDistance);
			if (dDistance > 2 * m_dMeanSquareError)
			{
				iCoarseCount++;
			}
		}

		fprintf(fp, "\n中误差：±%.6f米\n", m_dMeanSquareError);

		fprintf(fp, "\n粗差点个数:%d\n", iCoarseCount);
		fprintf(fp, "\n-----------------------------------------\n");
		fclose(fp);

		QgsMessageLog::logMessage(tr("生成精度检查报告成功！"), tr("矢量数据空间精度检查"), Qgis::Info);

		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("矢量数据空间精度检查"));
		msgBox.setText(tr("生成精度检查报告成功！"));
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Cancel);
		msgBox.exec();
		return;
	}
}