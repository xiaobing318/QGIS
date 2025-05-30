#ifndef QGS_VECTOR_DATA_CHECK_ODATA_PLUGIN_H
#define QGS_VECTOR_DATA_CHECK_ODATA_PLUGIN_H

#include "qgisplugin.h"
#include <QObject>

#include <string>
#include <vector>
using namespace std;

class QAction;
class QgisInterface;

class QgsVectorDataCheckOdataPlugin : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit QgsVectorDataCheckOdataPlugin(QgisInterface* qgisInterface);
	~QgsVectorDataCheckOdataPlugin() override;

public slots:

	//! 初始化Gui
	void initGui() override;
	//! actions
	
	// 生僻字统计
	void LayerRarewordStatistics();

	// 图层文件完整性检查
	void LayerIntegrityCheck();

	// 命名规范性检查
	void LayerNamingStandardizationCheck();

	// 数据编码规范性检查
	void LayerCodeStandardizationCheck();

	//! 卸载插件
	void unload() override;

private:
	
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin
	
	// “图层文件完整性检查”菜单
	QAction* mActionLayerIntegrityCheck = nullptr;

	// “生僻字统计”菜单
	QAction* mActionLayerRarewordStatistics = nullptr;

	// “命名规范性检查”菜单
	QAction* mActionLayerNamingStandardizationCheck = nullptr;

	// “数据编码规范性检查”菜单
	QAction* mActionLayerCodeStandardizationCheck = nullptr;

private slots:

	void updateActions();

private:



};

#endif // QGS_VECTOR_DATA_CHECK_ODATA_PLUGIN_H
