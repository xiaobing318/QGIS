#ifndef QGS_PLUGIN_VECTOR_ODATA2SHAPEFILE_H
#define QGS_PLUGIN_VECTOR_ODATA2SHAPEFILE_H

#pragma region "包含头文件（减少重复）"
/*--------------QGIS---------------*/
#include "qgisplugin.h"
/*--------------QGIS---------------*/

/*--------------QT---------------*/
#include <QObject>
/*--------------QT---------------*/

/*--------------STL---------------*/
#include <string>
#include <vector>
/*--------------STL---------------*/

using namespace std;
#pragma endregion

#pragma region "类的前向声明"
class QAction;
class QgisInterface;
#pragma endregion

#pragma region "矢量数据ODATA转化为Shapefile插件类"
class QgsPluginVectorOdata2Shapefile : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit QgsPluginVectorOdata2Shapefile(QgisInterface* qgisInterface);
	~QgsPluginVectorOdata2Shapefile() override;

public slots:

	//  初始化Gui
	void initGui() override;

	//  actions1:JBDX2Shapefile
	void JBDX2Shapefile();
	//  actions2:DZB2Shapefile
	void DZB2Shapefile();
	//  actions3:DZB2ShapefileWithSpecification
	void DZB2ShapefileWithSpecification();

	//  卸载插件
	void unload() override;

private:

	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;


	//!pointer to the qaction for this plugin

	// qaction1:JBDX2Shapefile
	QAction* mActionJBDX2Shapefile = nullptr;
  // qaction2:DZB2Shapefile
  QAction* mActionDZB2Shapefile = nullptr;
  // qaction2:DZB2ShapefileWithSpecification
  QAction* mActionDZB2ShapefileWithSpecification = nullptr;

private slots:
	void updateActions();

};
#pragma endregion

#endif // QGS_PLUGIN_VECTOR_ODATA2SHAPEFILE_H
