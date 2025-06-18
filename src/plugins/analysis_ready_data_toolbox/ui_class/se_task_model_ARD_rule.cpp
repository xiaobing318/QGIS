#define _HAS_STD_BYTE 0
#include "se_task_model_ARD_rule.h"

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
#include "se_customize_ARD.h"
#include "se_task_model_ARD_rule.h"

/*--------------STL---------------*/


using namespace std;

SE_TaskModelARDRuleDlg::SE_TaskModelARDRuleDlg(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);


	connect(ui.pushButton_OK, &QPushButton::clicked, this, &SE_TaskModelARDRuleDlg::pushButton_OK_clicked);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &SE_TaskModelARDRuleDlg::pushButton_Cancel_clicked);
	connect(ui.pushButton_Query, &QPushButton::clicked, this, &SE_TaskModelARDRuleDlg::pushButton_Query_clicked);
	connect(ui.pushButton_AddTask, &QPushButton::clicked, this, &SE_TaskModelARDRuleDlg::pushButton_AddTask_clicked);
	connect(ui.pushButton_DeleteTask, &QPushButton::clicked, this, &SE_TaskModelARDRuleDlg::pushButton_DeleteTask_clicked);

	connect(ui.pushButton_Open, &QPushButton::clicked, this, &SE_TaskModelARDRuleDlg::pushButton_Open_clicked);
	connect(ui.pushButton_Save, &QPushButton::clicked, this, &SE_TaskModelARDRuleDlg::pushButton_Save_clicked);
	// 设置右键菜单
	ui.treeWidget_TaskModelData->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.treeWidget_TaskModelData, &QTreeWidget::customContextMenuRequested, this, &SE_TaskModelARDRuleDlg::showContextMenu);

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_OutputDataPath->setText(m_qstrOutputDataPath);

	m_vTaskInfos.clear();
	m_vModelInfos.clear();
	m_vARDInfos.clear();

	// 颜色交叉
	ui.treeWidget_TaskModelData->setAlternatingRowColors(true);
}

SE_TaskModelARDRuleDlg::~SE_TaskModelARDRuleDlg()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("TASK_MODEL_ARD_RULE/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("TASK_MODEL_ARD_RULE/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);

	if (m_pModelsDlg)
	{
		delete m_pModelsDlg;
		m_pModelsDlg = nullptr;
	}

	if (m_pDatasDlg)
	{
		delete m_pDatasDlg;
		m_pDatasDlg = nullptr;
	}
}

void SE_TaskModelARDRuleDlg::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("TASK_MODEL_ARD_RULE/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrOutputDataPath = settings.value(QStringLiteral("TASK_MODEL_ARD_RULE/OutputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();




}


/*加载配置文件*/
void SE_TaskModelARDRuleDlg::pushButton_Open_clicked()
{

	// 选择xml属性配置文件
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("请选择任务-模型-数据映射规则配置文件");
	QString filter = tr("xml 文件(*.xml)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}
	else
	{
		ui.lineEdit_InputDataPath->setText(strFileName);
		m_qstrInputDataPath = strFileName;
		if (!LoadXmlConfigFile(strFileName.toLocal8Bit(), m_vTaskInfos))
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("打开任务-模型-数据映射规则配置文件失败！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}

		// 填充树控件
		InitTreeWidget();

	}
}

/*保存配置文件*/
void SE_TaskModelARDRuleDlg::pushButton_Save_clicked()
{
	SaveXMLConfig();
}

void SE_TaskModelARDRuleDlg::pushButton_OK_clicked()
{
	// 如果文件不存在
	if (!CheckFileOrDirExist(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_InputDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 如果文件不存在
	if (!CheckFileOrDirExist(ui.lineEdit_OutputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("文件：%1不存在！").arg(ui.lineEdit_OutputDataPath->text()));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}



	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("TASK_MODEL_ARD_RULE/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("TASK_MODEL_ARD_RULE/OutputDataPath"), m_qstrOutputDataPath, QgsSettings::Section::Plugins);
	accept();
}

void SE_TaskModelARDRuleDlg::pushButton_AddTask_clicked()
{
	QList<QString> allItemName{ tr("newTask"),tr("新增任务") };
	// 创建顶层Item-任务标识
	QTreeWidgetItem* newTaskItem = new QTreeWidgetItem(allItemName);
	newTaskItem->setFlags(newTaskItem->flags() | Qt::ItemIsEditable); // 设置子项可编辑
	ui.treeWidget_TaskModelData->addTopLevelItem(newTaskItem);

}

void SE_TaskModelARDRuleDlg::pushButton_DeleteTask_clicked()
{
	// 判断当前的选取的是否是任务名称
	// 如果是任务，则可以删除该节点
	QTreeWidgetItem* currentItem = ui.treeWidget_TaskModelData->currentItem();
	if (currentItem)
	{
		// 判断当前节点是否是任务节点
		int iLevel = GetItemLevel(currentItem);

		// 如果是0级，任务节点
		// 则可以删除
		if (iLevel == 0)
		{
			// 删除选中的项及其所有子项
			deleteItem(currentItem);

		}
		else
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
			msgBox.setText(tr("请选择任务节点！"));
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();
			return;
		}
	}
	else
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("请选择任务节点！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

}

void SE_TaskModelARDRuleDlg::pushButton_Query_clicked()
{
	// 获取树控件的任务列表
	vector<TaskInfo> vTaskInfos;
	vTaskInfos.clear();
	GetTaskInfosFromTreeWidget(vTaskInfos);

	// 满足条件的任务个数
	int iResultCount = 0;

	for (int i = 0; i < vTaskInfos.size(); ++i)
	{
		// 如果包含任务名称
		if (vTaskInfos[i].strTaskIdentify.contains(ui.lineEdit_TaskName->text()))
		{
			ui.treeWidget_TaskModelData->topLevelItem(i)->setBackground(0, QColor(173, 216, 230)); // Light Blue
			ui.treeWidget_TaskModelData->topLevelItem(i)->setBackground(1, QColor(173, 216, 230)); // Light Blue
			iResultCount++;
		}
		else
		{
			ui.treeWidget_TaskModelData->topLevelItem(i)->setBackground(0, QColor(255, 255, 255)); // 白色
			ui.treeWidget_TaskModelData->topLevelItem(i)->setBackground(1, QColor(255, 255, 255)); // 白色
		}
	}

	// 查询结果弹窗
	if (iResultCount == 0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("满足查询条件的任务个数为0！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}
}

void SE_TaskModelARDRuleDlg::pushButton_Cancel_clicked()
{
	reject();
}



// 读取属性检查配置信息
bool SE_TaskModelARDRuleDlg::LoadXmlConfigFile(const char* szConfigXmlFile, vector<TaskInfo>& vTaskInfos)
{
	// 如果xml文件为空
	if (!szConfigXmlFile)
	{
		return false;
	}

	vTaskInfos.clear();

	// 读取xml文件
	xmlDocPtr doc = nullptr;
	xmlNodePtr pRootNode = nullptr;

	// 配置文件
	doc = xmlParseFile(szConfigXmlFile);

	if (nullptr == doc)
	{
		return false;
	}

	// 获取根节点<task_model_data_list>
	pRootNode = xmlDocGetRootElement(doc);

	if (NULL == pRootNode) {
		xmlFreeDoc(doc);
		return false;
	}

	// 遍历所有根节点的子节点
	xmlNodePtr cur;

	//遍历处理根节点的每一个子节点
	cur = pRootNode->xmlChildrenNode;
	xmlChar* key;
	while (cur != NULL)
	{
		
		// 任务task节点
		if (!xmlStrcmp(cur->name, (const xmlChar*)"task"))
		{
			TaskInfo info;
			Parse_task(doc, cur, info);
			vTaskInfos.push_back(info);
		}

		cur = cur->next;
	}

	xmlFreeDoc(doc);

	return true;
}

// 解析每个task节点
void SE_TaskModelARDRuleDlg::Parse_task(
	xmlDocPtr doc,
	xmlNodePtr cur,
	TaskInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != NULL)
	{
		// task_identify：任务标识
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"task_identify")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strTaskIdentify = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// task_name：任务名称
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"task_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strTaskName = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// model：模型列表
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"model")))
		{
			ModelInfo model_info;
			Parse_model(doc, cur, model_info);
			info.vModelInfos.push_back(model_info);
		}
		cur = cur->next;
	}
}

// 解析每个model节点
void SE_TaskModelARDRuleDlg::Parse_model(
	xmlDocPtr doc,
	xmlNodePtr cur,
	ModelInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// model_identify：模型标识
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"model_identify")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strModelIdentify = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// model_name：模型名称
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"model_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strModelName = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// data：数据
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"data")))
		{
			ARDInfo ard_info;
			Parse_data(doc, cur, ard_info);
			info.vARDInfos.push_back(ard_info);
		}

		

		cur = cur->next;
	}
}


// 解析每个data节点
void SE_TaskModelARDRuleDlg::Parse_data(
	xmlDocPtr doc,
	xmlNodePtr cur,
	ARDInfo& info)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != nullptr)
	{
		// data_identify：分析就绪型数据标识
		if ((!xmlStrcmp(cur->name, (const xmlChar*)"data_identify")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strARDIdentify = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		// data_name：分析就绪型数据名称
		else if ((!xmlStrcmp(cur->name, (const xmlChar*)"data_name")))
		{
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			info.strARDName = QString::fromUtf8((char*)key);
			xmlFree(key);
		}

		cur = cur->next;
	}
}

void SE_TaskModelARDRuleDlg::InitTreeWidget()
{
	// 创建任务标识列表
	for (int i = 0; i < m_vTaskInfos.size(); ++i)
	{
		QList<QString> allItemName{ m_vTaskInfos[i].strTaskIdentify,m_vTaskInfos[i].strTaskName};
		// 创建顶层Item-任务标识
		QTreeWidgetItem* newTaskItem = new QTreeWidgetItem(allItemName);
		newTaskItem->setFlags(newTaskItem->flags() | Qt::ItemIsEditable); // 设置子项可编辑
		ui.treeWidget_TaskModelData->addTopLevelItem(newTaskItem);
	}

	for (int i = 0; i < ui.treeWidget_TaskModelData->topLevelItemCount(); ++i)
	{
		// 获取顶层节点
		QTreeWidgetItem* topItem = ui.treeWidget_TaskModelData->topLevelItem(i);
		
		// 插入多个模型
		for (int iModelIndex = 0; iModelIndex < m_vTaskInfos[i].vModelInfos.size(); ++iModelIndex)
		{
			// 创建模型子节点
			QTreeWidgetItem* childItem = new QTreeWidgetItem(QStringList{ m_vTaskInfos[i].vModelInfos[iModelIndex].strModelIdentify,m_vTaskInfos[i].vModelInfos[iModelIndex].strModelName});
			childItem->setFlags(childItem->flags() | Qt::ItemIsEditable); // 设置子项可编辑
			
			// 将模型子节点添加到顶层节点上
			topItem->addChild(childItem);

			
			// 每个插入多个数据
			for (int iDataIndex = 0; iDataIndex < m_vTaskInfos[i].vModelInfos[iModelIndex].vARDInfos.size(); ++iDataIndex)
			{
				// 创建数据子节点
				QTreeWidgetItem* dataItem = new QTreeWidgetItem(QStringList{ 
					m_vTaskInfos[i].vModelInfos[iModelIndex].vARDInfos[iDataIndex].strARDIdentify,
					m_vTaskInfos[i].vModelInfos[iModelIndex].vARDInfos[iDataIndex].strARDName});
				dataItem->setFlags(dataItem->flags() | Qt::ItemIsEditable); // 设置子项可编辑
				childItem->addChild(dataItem);
			}
		}
	}
	
	ui.treeWidget_TaskModelData->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	// 设置标题默认居中
	ui.treeWidget_TaskModelData->header()->setDefaultAlignment(Qt::AlignHCenter); 



}


void SE_TaskModelARDRuleDlg::GetTaskInfosFromTreeWidget(vector<TaskInfo>& vTaskInfos)
{
	// 遍历树控件的所有项
	for (int i = 0; i < ui.treeWidget_TaskModelData->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* topItem = ui.treeWidget_TaskModelData->topLevelItem(i);
		
		TaskInfo info;
		// 任务标识
		info.strTaskIdentify = topItem->text(0);
		
		// 任务名称
		info.strTaskName = topItem->text(1);

		int iModelCount = topItem->childCount();

		// 如果有子节点
		if (iModelCount > 0)
		{
			
			for (int iModelIndex = 0; iModelIndex < iModelCount; ++iModelIndex)
			{
				ModelInfo modelInfo;

				QTreeWidgetItem* ModelItem = topItem->child(iModelIndex);
				modelInfo.strModelIdentify = ModelItem->text(0);
				modelInfo.strModelName = ModelItem->text(1);

				// 如果节点数大于0
				int iDataCount = ModelItem->childCount();
				if (iDataCount > 0)
				{
					for (int iDataIndex = 0; iDataIndex < iDataCount; ++iDataIndex)
					{
						QTreeWidgetItem* dataItem = ModelItem->child(iDataIndex);
						
						ARDInfo ardInfo;

						ardInfo.strARDIdentify = dataItem->text(0);
						ardInfo.strARDName = dataItem->text(1);
						
						modelInfo.vARDInfos.push_back(ardInfo);
					}

					// 如果数据节点个数大于0
					if (modelInfo.vARDInfos.size() > 0)
					{
						info.vModelInfos.push_back(modelInfo);
					}
				}
			}

			// 如果模型大于0
			if (info.vModelInfos.size() > 0)
			{
				vTaskInfos.push_back(info);
			}
		}
	}
}

void SE_TaskModelARDRuleDlg::SaveXMLConfig()
{
	// 获取树控件中所有节点的信息构建TaskInfo
	vector<TaskInfo> vTaskInfos;
	vTaskInfos.clear();

	GetTaskInfosFromTreeWidget(vTaskInfos);

	if (vTaskInfos.size() == 0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("任务-模型-数据映射规则配置规则数为0！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// QDomDocument类
	QDomDocument doc;
	QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
	doc.appendChild(instruction);

	// 创建根节点  QDomElemet元素:task_model_data_list，任务列表
	QDomElement root = doc.createElement("task_model_data_list");

	// 添加根节点
	doc.appendChild(root);

	// 遍历每个任务
	for (int iTaskIndex = 0; iTaskIndex < vTaskInfos.size(); ++iTaskIndex)
	{
		TaskInfo info = vTaskInfos[iTaskIndex];


		// 创建任务Task节点
		// 子节点<task>
		QDomElement TaskNode = doc.createElement("task");
		root.appendChild(TaskNode);

		// 【1】创建任务标识：task_identify节点
		QDomElement TaskIdentifyNode = doc.createElement("task_identify");
		TaskNode.appendChild(TaskIdentifyNode);
		QDomText strTaskIdentifyText = doc.createTextNode(info.strTaskIdentify);
		TaskIdentifyNode.appendChild(strTaskIdentifyText);

		// 【2】创建任务名称：task_name节点
		QDomElement TaskNameNode = doc.createElement("task_name");
		TaskNode.appendChild(TaskNameNode);
		QDomText strTaskNameText = doc.createTextNode(info.strTaskName);
		TaskNameNode.appendChild(strTaskNameText);

		// 【3】创建每个model节点
		if (info.vModelInfos.size() > 0)
		{
			// 模型标识节点
			for (int iModelIndex = 0; iModelIndex < info.vModelInfos.size(); ++iModelIndex)
			{
				QDomElement ModelNode = doc.createElement("model");
				TaskNode.appendChild(ModelNode);

				// 【4】模型标识节点model_identify
				QDomElement ModelIdentifyNode = doc.createElement("model_identify");
				ModelNode.appendChild(ModelIdentifyNode);
				QDomText strModelIdentifyText = doc.createTextNode(info.vModelInfos[iModelIndex].strModelIdentify);
				ModelIdentifyNode.appendChild(strModelIdentifyText);

				// 【5】模型名称节点model_name
				QDomElement ModelNameNode = doc.createElement("model_name");
				ModelNode.appendChild(ModelNameNode);
				QDomText strModelNameText = doc.createTextNode(info.vModelInfos[iModelIndex].strModelName);
				ModelNameNode.appendChild(strModelNameText);

				// 【6】创建每个data 节点
				if (info.vModelInfos[iModelIndex].vARDInfos.size() > 0)
				{
					for (int iDataIndex = 0; iDataIndex < info.vModelInfos[iModelIndex].vARDInfos.size(); ++iDataIndex)
					{
						QDomElement DataNode = doc.createElement("data");
						ModelNode.appendChild(DataNode);

						ARDInfo ardInfo = info.vModelInfos[iModelIndex].vARDInfos[iDataIndex];

						// 【7】数据标识节点data_identify
						QDomElement DataIdentifyNode = doc.createElement("data_identify");
						DataNode.appendChild(DataIdentifyNode);
						QDomText strDataIdentifyText = doc.createTextNode(ardInfo.strARDIdentify);
						DataIdentifyNode.appendChild(strDataIdentifyText);

						// 【8】数据名称节点data_name
						QDomElement DataNameNode = doc.createElement("data_name");
						DataNode.appendChild(DataNameNode);
						QDomText strDataNameText = doc.createTextNode(ardInfo.strARDName);
						DataNameNode.appendChild(strDataNameText);
					}
				}

			}
		}
	}

	// 配置文件默认保存在当前运行环境下
	QString curPath = m_qstrOutputDataPath;
	QString dlgTitle = tr("保存任务-模型-数据映射关系配置文件");
	QString filter = tr("xml 文件(*.xml)");
	QString strFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!strFileName.isEmpty())
	{
		ui.lineEdit_OutputDataPath->setText(strFileName);
		m_qstrOutputDataPath = strFileName;
		string strBaseName = CPLGetBasename(strFileName.toLocal8Bit());

		QFile file(strFileName);
		QDomText strNameText = doc.createTextNode(QString::fromLocal8Bit(strBaseName.c_str()));

		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
			return;
		QTextStream out(&file);
		out.setCodec("UTF-8");
		doc.save(out, 4, QDomNode::EncodingFromTextStream);
		file.close();


		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("数据包定制及标准化封装工具"));
		msgBox.setText(tr("任务-模型-数据映射规则配置文件保存成功！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


}


// 显示右键菜单
void SE_TaskModelARDRuleDlg::showContextMenu(const QPoint& pos)
{
	// 任务节点级别—0；模型级别—1；数据级别—2
	QTreeWidgetItem* item = ui.treeWidget_TaskModelData->itemAt(pos);
	if (item) 
	{
		// 获取item的级别
		int iItemLevel = GetItemLevel(item);

		// 如果是任务级别
		if (iItemLevel == 0)
		{
			QMenu contextMenu(tr("编辑任务"), this);
			QAction addAction("编辑任务", this);
			connect(&addAction, &QAction::triggered, this, &SE_TaskModelARDRuleDlg::addTaskChildNode);
			contextMenu.addAction(&addAction);
			contextMenu.exec(ui.treeWidget_TaskModelData->viewport()->mapToGlobal(pos));
		}

		// 如果是模型级别
		else if (iItemLevel == 1)
		{
			QMenu contextMenu(tr("编辑模型"), this);
			QAction addAction("编辑模型", this);
			connect(&addAction, &QAction::triggered, this, &SE_TaskModelARDRuleDlg::addModelChildNode);
			contextMenu.addAction(&addAction);

			QAction deleteAction("删除模型", this);
			connect(&deleteAction, &QAction::triggered, this, &SE_TaskModelARDRuleDlg::deleteTaskChildNode);
			contextMenu.addAction(&deleteAction);

			contextMenu.exec(ui.treeWidget_TaskModelData->viewport()->mapToGlobal(pos));
		}
	}
}

void SE_TaskModelARDRuleDlg::addTaskChildNode()
{
	m_pModelsDlg = new SE_ModelsDlg(nullptr, Qt::WindowCloseButtonHint);
	m_pModelsDlg->exec();


	// 判断对话框的返回值
	if (m_pModelsDlg->result() == QDialog::Accepted) 
	{
		m_pModelsDlg->GetModelInfosFromModelDlg(m_vModelDlgIdentifys, m_vModelDlgNames);
		
		QList<QTreeWidgetItem*> selectedItems = ui.treeWidget_TaskModelData->selectedItems();
		if (!selectedItems.isEmpty()) 
		{
			QTreeWidgetItem* selectedItem = selectedItems.first(); // 获取第一个选中的节点
			// 创建模型子节点
	
			vector<QString> vTreeModelIdentifys;
			vTreeModelIdentifys.clear();

			// 获取当前节点的所有模型标识 
			GetChildItems(selectedItem, vTreeModelIdentifys);

			// 与当前节点下的模型进行判断，如果不存在，则添加
			for (int i = 0; i < m_vModelDlgIdentifys.size(); ++i)
			{
				if (!contains(vTreeModelIdentifys, m_vModelDlgIdentifys[i])) 
				{
					// 创建模型节点
					QTreeWidgetItem* childItem = new QTreeWidgetItem(QStringList{ 
						m_vModelDlgIdentifys[i],
						m_vModelDlgNames[i] });
					childItem->setFlags(childItem->flags() | Qt::ItemIsEditable); // 设置子项可编辑

					// 将模型子节点添加到顶层节点上
					selectedItem->addChild(childItem);
				}
			}


		}
	}

	ui.treeWidget_TaskModelData->update();
}

void SE_TaskModelARDRuleDlg::addModelChildNode()
{
	m_pDatasDlg = new SE_DatasDlg(nullptr, Qt::WindowCloseButtonHint);
	m_pDatasDlg->exec();


	// 判断对话框的返回值
	if (m_pDatasDlg->result() == QDialog::Accepted)
	{
		m_pDatasDlg->GetDataInfosFromDataDlg(m_vDataDlgIdentifys, m_vDataDlgNames);

		QList<QTreeWidgetItem*> selectedItems = ui.treeWidget_TaskModelData->selectedItems();
		if (!selectedItems.isEmpty())
		{
			QTreeWidgetItem* selectedItem = selectedItems.first(); // 获取第一个选中的节点
			// 创建模型子节点

			vector<QString> vTreeDataIdentifys;
			vTreeDataIdentifys.clear();

			// 获取当前节点的所有数据标识 
			GetChildItems(selectedItem, vTreeDataIdentifys);

			// 与当前节点下的模型进行判断，如果不存在，则添加
			for (int i = 0; i < m_vDataDlgIdentifys.size(); ++i)
			{
				if (!contains(vTreeDataIdentifys, m_vDataDlgIdentifys[i]))
				{
					// 创建模型节点
					QTreeWidgetItem* childItem = new QTreeWidgetItem(QStringList{
						m_vDataDlgIdentifys[i],
						m_vDataDlgNames[i] });
					childItem->setFlags(childItem->flags() | Qt::ItemIsEditable); // 设置子项可编辑

					// 将模型子节点添加到顶层节点上
					selectedItem->addChild(childItem);
				}
			}


		}
	}

	ui.treeWidget_TaskModelData->update();

}

void SE_TaskModelARDRuleDlg::deleteTaskChildNode()
{
	QList<QTreeWidgetItem*> selectedItems = ui.treeWidget_TaskModelData->selectedItems();
	if (!selectedItems.isEmpty())
	{
		QTreeWidgetItem* selectedItem = selectedItems.first(); // 获取第一个选中的节点

		// 删除选中的项及其所有子项
		deleteItem(selectedItem);
	}	
}



bool SE_TaskModelARDRuleDlg::contains(const vector<QString>& vec, const QString& str) 
{
	return std::find(vec.begin(), vec.end(), str) != vec.end();
}

#pragma region  "检查文件或文件夹是否存在"

bool SE_TaskModelARDRuleDlg::CheckFileOrDirExist(const QString& qstrFileDirOrPath)
{
	QFileInfo objFileInfo(qstrFileDirOrPath);
	return objFileInfo.exists(); // 直接检查文件或目录是否存在
}

#pragma endregion
