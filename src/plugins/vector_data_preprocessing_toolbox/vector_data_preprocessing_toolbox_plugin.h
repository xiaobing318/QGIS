#ifndef VECTOR_DATA_PREPROCESSING_TOOLBOX_H
#define VECTOR_DATA_PREPROCESSING_TOOLBOX_H

#include "qgisplugin.h"
#include <QObject>

#include <string>
#include <vector>
using namespace std;

class QAction;
class QgisInterface;

class QgsCoordinateTransformationToolBox : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit QgsCoordinateTransformationToolBox(QgisInterface* qgisInterface);
	~QgsCoordinateTransformationToolBox() override;

public slots:

	//! 初始化Gui
	void initGui() override;
	//! actions
	
	// 地理坐标系转换
	void GeoCoordSysTrans();

	// 投影坐标系转换
	void ProjCoordSysTrans();

  // 矢量源数据检查工具
  void source_data_inspection_tool();

	//! 卸载插件
	void unload() override;

private:
	
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin
	
	// “地理坐标系转换”菜单
	QAction* mAction_GeoCoordSysTrans = nullptr;

	// “投影坐标系转换”菜单（目前还没有实现）
	QAction* mAction_ProjCoordSysTrans = nullptr;

  // “矢量源数据检查”菜单
  QAction* mAction_source_data_inspection_tool = nullptr;

private slots:

	void updateActions();

private:



};

#endif // VECTOR_DATA_PREPROCESSING_TOOLBOX_H
