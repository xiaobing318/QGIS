#ifndef CONSISTENCY_PROCESSING_TOOLBOX_PLUGIN_H
#define CONSISTENCY_PROCESSING_TOOLBOX_PLUGIN_H

#pragma region "包含头文件（减少重复）"
/*------------------QGIS相关的头文件-------------------*/
#include "../qgisplugin.h"
/*------------------QGIS相关的头文件-------------------*/


/*-------------要素一致性处理工具相关头文件-------------*/
#include "consistency_matching_tool.h"
//#include "consistency_attribute_connection_processing_tool.h"
//#include "consistency_matching_form_checking_tool.h"
//#include "consistent_geometry_editing_and_processing_tool.h"
/*-------------要素一致性处理工具相关头文件-------------*/

/*-------------QT-------------*/
#include <QObject>
/*-------------QT-------------*/
#pragma endregion

#pragma region "类的前向申明"
class QAction;
class QgisInterface;
class QObject;
#pragma endregion

#pragma region "QgsConsistencyProcessingToolboxPlugin插件类"
class QgsConsistencyProcessingToolboxPlugin : public QObject, public QgisPlugin
{
  Q_OBJECT
#pragma region "插件公开API接口"
public:
  explicit QgsConsistencyProcessingToolboxPlugin(QgisInterface* qgisInterface);
  ~QgsConsistencyProcessingToolboxPlugin() override;

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
  //  1、一致性匹配工具（自动化）
  void consistency_matching_tool();
  //  2、一致性几何编辑处理工具（人机交互）
  void consistent_geometry_editing_and_processing_tool();
  //  3、一致性匹配表单检查工具（人工交互）
  void consistency_matching_form_checking_tool();
  //  4、一致性属连接处理工具（人工交互）
  void consistency_attribute_connection_processing_tool();
  
#pragma endregion

#pragma region "插件私有数据成员"
private:
  //! Pointer to the QGIS interface object
  QgisInterface* mQGisIface = nullptr;


  //  1、一致性匹配工具（自动化）
  QAction* mAction_consistency_matching_tool = nullptr;
  //  2、一致性几何编辑处理工具（人机交互）
  QAction* mAction_consistent_geometry_editing_and_processing_tool = nullptr;
  //  3、一致性匹配表单检查工具（人工交互）
  QAction* mAction_consistency_matching_form_checking_tool = nullptr;
  //  4、一致性属连接处理工具（人工交互）
  QAction* mAction_consistency_attribute_connection_processing_tool = nullptr;
#pragma endregion

};
#pragma endregion

#endif // CONSISTENCY_PROCESSING_TOOLBOX_PLUGIN_H
