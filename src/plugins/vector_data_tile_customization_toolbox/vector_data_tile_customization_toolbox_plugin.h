#ifndef VECTOR_DATA_TILE_CUSTOMIZATION_TOOLBOX_H
#define VECTOR_DATA_TILE_CUSTOMIZATION_TOOLBOX_H

#include "qgisplugin.h"
#include <QObject>

#include <string>
#include <vector>
using namespace std;

class QAction;
class QgisInterface;

class vector_data_tile_customization_toolbox : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit vector_data_tile_customization_toolbox(QgisInterface* qgisInterface);
	~vector_data_tile_customization_toolbox() override;

public slots:

	//! 初始化Gui
	void initGui() override;
	//! actions
	
	// 	1、网址跳转工具
	void URL_jump();

	// 	2、其他工具

	//! 卸载插件
	void unload() override;

private:
	
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin
	
	// 1、网址跳转工具
	QAction* mAction_url_jum = nullptr;


private slots:

	void updateActions();

private:



};

#endif // VECTOR_DATA_TILE_CUSTOMIZATION_TOOLBOX_H
