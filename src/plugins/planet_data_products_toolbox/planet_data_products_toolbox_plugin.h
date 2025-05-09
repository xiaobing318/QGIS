#ifndef PLANET_DATA_PRODUCTS_TOOLBOX_PLUGIN_H
#define PLANET_DATA_PRODUCTS_TOOLBOX_PLUGIN_H

#pragma region "包含头文件（减少重复）"
/*------------------QGIS相关的头文件-------------------*/
#include "../qgisplugin.h"
/*------------------QGIS相关的头文件-------------------*/


/*-------------星球数据产品工具集相关头文件-------------*/
//  1、矢量数据翻译工具
#include "vector_data_translation_tool.h"
//  2、分析就绪数据处理工具

//  3、点要素分级处理工具
#include "point_feature_hierarchical_processing_tool.h"
/*-------------星球数据产品工具集相关头文件-------------*/

/*-------------QT-------------*/
#include <QObject>
/*-------------QT-------------*/
#pragma endregion

#pragma region "类的前向申明"
class QAction;
class QgisInterface;
class QObject;
#pragma endregion

#pragma region "QgsPlanetDataProductsToolboxPlugin插件类"
class QgsPlanetDataProductsToolboxPlugin : public QObject, public QgisPlugin
{
  Q_OBJECT
#pragma region "插件公开API接口"
public:
  explicit QgsPlanetDataProductsToolboxPlugin(QgisInterface* qgisInterface);
  ~QgsPlanetDataProductsToolboxPlugin() override;

#pragma endregion

#pragma region "插件公开槽函数API接口"
public slots:
  //! init the gui
  void initGui() override;
  //! unload the plugin
  void unload() override;
  //! show the help document
  void help();

  //! actions
  //  1、矢量数据翻译工具
  void vector_data_translation_tool();
  //  2、分析就绪数据处理工具（临时：给到空天院）
  void analysis_ready_data_process_tool();
  //  3、点要素分级处理工具
  void point_feature_hierarchical_processing_tool();
  //  4、其他工具
  
#pragma endregion

#pragma region "插件私有数据成员"
private:
  //! Pointer to the QGIS interface object
  QgisInterface* mQGisIface = nullptr;


  //  1、矢量数据翻译工具
  QAction* mAction_vector_data_translation_tool = nullptr;
  //  2、分析就绪数据处理工具（临时：给到空天院）
  QAction* mAction_analysis_ready_data_process_tool = nullptr;
  //  3、点要素分级处理工具
  QAction* mAction_point_feature_hierarchical_processing_tool = nullptr;
  //  4、其他工具
  QAction* mAction_other_tools = nullptr;

#pragma endregion

};
#pragma endregion

#endif // PLANET_DATA_PRODUCTS_TOOLBOX_PLUGIN_H
