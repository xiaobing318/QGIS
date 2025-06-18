#ifndef SE_TASK_MODEL_ARD_RULE
#define SE_TASK_MODEL_ARD_RULE

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_task_model_ARD_rule.h"
#include "../ui_class/se_models_dlg.h"
#include "../ui_class/se_datas_dlg.h"
#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"

#include <qstringlist.h>
/*读取xml头文件*/
#include <libxml/parser.h>
using namespace std;

/*分析就绪数据信息结构体*/
struct ARDInfo
{
	QString				strARDIdentify;			// 分析就绪数据标识
	QString				strARDName;				// 分析就绪数据名称

	ARDInfo()
	{
		strARDIdentify = "";
		strARDName = "";
	}
};



/*模型信息结构体*/
struct ModelInfo
{
	QString				strModelIdentify;		// 模型标识
	QString				strModelName;			// 模型名称
	vector<ARDInfo>		vARDInfos;				// 模型关联的分析就绪数据列表

	ModelInfo()
	{
		strModelIdentify = "";
		strModelName = "";
		vARDInfos.clear();
	}

};


/*任务-模型-数据映射规则结构体*/
struct TaskInfo
{
	QString					strTaskIdentify;		// 任务标识	
	QString					strTaskName;			// 任务名称
	vector<ModelInfo>		vModelInfos;			// 当前任务关联的模型列表

	TaskInfo()
	{
		strTaskIdentify = "";
		strTaskName = "";
		vModelInfos.clear();
	}
};


class SE_TaskModelARDRuleDlg : public QDialog
{
	Q_OBJECT

public:
	SE_TaskModelARDRuleDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_TaskModelARDRuleDlg() override;

	void restoreState();

private:

	Ui_Task_Model_ARD_RuleDlg ui;

private slots:

	// 加载配置文件
	void pushButton_Open_clicked();

	// 保存配置文件
	void pushButton_Save_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 增加任务
	void pushButton_AddTask_clicked();

	// 删除任务
	void pushButton_DeleteTask_clicked();

	// 查找任务
	void pushButton_Query_clicked();

	// 取消
	void pushButton_Cancel_clicked();

	// 显示右键菜单
	void showContextMenu(const QPoint& pos);

	// 增加任务节点的子节点，编辑任务
	void addTaskChildNode();

	// 增加模型节点的子节点，编辑模型
	void addModelChildNode();

	// 增加任务节点的子节点，删除模型
	void deleteTaskChildNode();

private:

	// 输入配置文件路径
	QString m_qstrInputDataPath;

	// 输出配置文件路径
	QString m_qstrOutputDataPath;

	// 任务信息
	vector<TaskInfo> m_vTaskInfos;

	// 初始化模型信息
	vector<ModelInfo> m_vModelInfos;

	// 分析就绪数据信息
	vector<ARDInfo> m_vARDInfos;

	// 从模型对话框获取的模型标识
	vector<QString> m_vModelDlgIdentifys;

	// 从模型对话框获取的模型名称
	vector<QString> m_vModelDlgNames;

	// 从数据对话框获取的数据标识
	vector<QString> m_vDataDlgIdentifys;

	// 从数据对话框获取的数据名称
	vector<QString> m_vDataDlgNames;

	// 模型对话框
	SE_ModelsDlg* m_pModelsDlg;

	// 分析就绪型数据对话框
	SE_DatasDlg* m_pDatasDlg;
private:

	// 加载映射规则配置文件
	bool LoadXmlConfigFile(const char* szConfigXmlFile, vector<TaskInfo>& vTaskInfos);
	
	// 解析task节点
	void Parse_task(xmlDocPtr doc, xmlNodePtr cur, TaskInfo& info);

	// 解析model节点
	void Parse_model(xmlDocPtr doc, xmlNodePtr cur, ModelInfo& info);

	// 解析data节点
	void Parse_data(xmlDocPtr doc, xmlNodePtr cur, ARDInfo& info);

	// 将任务-模型-数据填充到树控件中
	void InitTreeWidget();

	// 从树控件中获取所有任务信息
	void GetTaskInfosFromTreeWidget(vector<TaskInfo>& vTaskInfos);

	// 保存xml配置文件
	void SaveXMLConfig();

	// 获取当前项的层级
	inline int GetItemLevel(QTreeWidgetItem* item) {
		int level = 0;
		while (item->parent()) {
			item = item->parent();
			level++;
		}
		return level;
	}

	// 删除当前节点下所有的节点
	inline void deleteItem(QTreeWidgetItem* item) 
	{
		// 递归删除子项
		while (item->childCount() > 0) {
			deleteItem(item->child(0));  // 递归删除第一个子项
		}
		delete item;  // 删除当前项
	}

	// 获取当前节点下的所有子节点
	inline void GetChildItems(QTreeWidgetItem* item, vector<QString>& vItemIdentifys)
	{
		vItemIdentifys.clear();

		// 如果节点数大于0
		int iChildCount = item->childCount();
		if (iChildCount > 0)
		{
			for (int iIndex = 0; iIndex < iChildCount; ++iIndex)
			{
				QTreeWidgetItem* childItem = item->child(iIndex);
				if (childItem)
				{
					vItemIdentifys.push_back(childItem->text(0));
				}
				
			}
		}
	}

	// 从字符串数组中查找指定字符串
	bool contains(const vector<QString>& vec, const QString& str);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);
};

#endif // SE_TASK_MODEL_ARD_RULE
