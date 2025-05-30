#ifndef QGS_VECTOR_DATA_CHECK_SHP_PLUGIN_H
#define QGS_VECTOR_DATA_CHECK_SHP_PLUGIN_H

#include "qgisplugin.h"
#include <QObject>

#include <string>
#include <vector>
using namespace std;

class QAction;
class QgisInterface;

class QgsVectorDataCheckShpPlugin : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit QgsVectorDataCheckShpPlugin(QgisInterface* qgisInterface);
	~QgsVectorDataCheckShpPlugin() override;

public slots:

	//! 初始化Gui
	void initGui() override;
	//! actions
	
	// 要素属性检查
	void FeatureAttributeCheck();

	// 要素范围检查
	void FeatureExtentCheck();

	// 要素分类统计
	void FeatureCategoryStatistics();

	// 图层文件可用性检查
	void LayerValidityCheck();

	// 图层文件完整性检查
	void LayerIntegrityCheck();

	// 一体化数据属性检查
	void FeatureAttributeCheckYTH();

	//! 卸载插件
	void unload() override;

private:
	
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin
	
	// “要素属性检查”菜单
	QAction* mActionFeatureAttributeCheck = nullptr;

	// “要素范围检查”菜单
	QAction* mActionFeatureExtentCheck = nullptr;

	// “要素分类统计”菜单
	QAction* mActionFeatureCategoryStatistics = nullptr;

	// “图层文件可用性检查”菜单
	QAction* mActionLayerValidityCheck = nullptr;

	// “图层文件完整性检查”菜单
	QAction* mActionLayerIntegrityCheck = nullptr;

	// “一体化数据属性检查”菜单
	QAction* mActionFeatureAttributeCheckYTH = nullptr;

private slots:

	void updateActions();

private:



};

#endif // QGS_VECTOR_DATA_CHECK_SHP_PLUGIN_H
