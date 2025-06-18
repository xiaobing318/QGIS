#ifndef SE_MANAGE_ARD_PACKAGE
#define SE_MANAGE_ARD_PACKAGE

#include <QDialog>

#include "commontype/se_config.h"
#include "ui_manage_ARD_package.h"
#include <vector>

#include "cpl_config.h"
#include "gdal_priv.h"
#include <gdal.h>
#include "cpl_conv.h"
#include "commontype/se_commontypedef.h"

#include <qstringlist.h>
/*读取xml头文件*/
#include <libxml/parser.h>

#include "sqlite3.h"

using namespace std;

/*分析就绪型数据包元数据结构体*/
struct ARD_Package_MetaData_Info
{
	// 分析就绪型数据产品标识列表
	vector<string> vIdentifys;

	// 分析就绪型数据产品名称列表
	vector<string> vNames;

	// 分析就绪型数据级别
	vector<int>	vDataLevels;

	// 分析就绪型数据类型
	vector<string> vDataTypes;

	// 分析就绪数据覆盖范围
	vector<SE_DRect> vdRects;

	// 描述信息
	vector<string> vDetails;

	// 支撑的任务
	vector<string> vTasks;

	// 时相信息
	vector<string> vTemporals;

	ARD_Package_MetaData_Info()
	{
		vIdentifys.clear();
		vNames.clear();
		vDataLevels.clear();
		vDataTypes.clear();
		vdRects.clear();
		vDetails.clear();
		vTasks.clear();
		vTemporals.clear();
	}

};






class SE_ManageARDPackageDlg : public QDialog
{
	Q_OBJECT

public:
	SE_ManageARDPackageDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~SE_ManageARDPackageDlg() override;

	void restoreState();

private:

	Ui_Manage_ARD_PackageDlg ui;

private slots:

	// 加载配置文件
	void pushButton_Open_clicked();

	// 确定
	void pushButton_OK_clicked();

	// 查找任务
	void pushButton_Query_clicked();

	// 取消
	void pushButton_Cancel_clicked();

	// 显示右键菜单
	void showContextMenu(const QPoint& pos);


	// 增加任务节点的子节点，删除模型
	void deletePackageChildNode();

	// 输入数据路径改变
	void on_lineEdit_InputDataPath_TextChanged(const QString& qstr);

	// 删除指定条件的栅格数据包元数据项
	bool DeleteRowsFromMetadata(sqlite3* db, const std::string& condition);

	// 删除指定条件的矢量数据包元数据
	bool DeleteFeaturesByAttribute(const std::string& datasourcePath, const std::string& layerName, const std::string& attributeName, const std::string& attributeValue);

	// 检查文件或文件夹是否存在
	bool CheckFileOrDirExist(const QString& qstrFileDirOrPath);

private:

	// 输入配置文件路径
	QString m_qstrInputDataPath;

	// 输出配置文件路径
	QString m_qstrOutputDataPath;

	// 从模型对话框获取的模型标识
	vector<QString> m_vModelDlgIdentifys;

	// 从模型对话框获取的模型名称
	vector<QString> m_vModelDlgNames;

	// 从数据对话框获取的数据标识
	vector<QString> m_vDataDlgIdentifys;

	// 从数据对话框获取的数据名称
	vector<QString> m_vDataDlgNames;

	// 矢量分析就绪型数据包标志
	bool m_bVectorGpkgPackage;

private:

	// 从树控件获取顶级节点
	void GetPackageInfosFromTreeWidget(vector<QString>& vInfos);

	// 将任务-模型-数据填充到树控件中
	void InitTreeWidget();


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

	// 获取gpkg文件列表
	QStringList GetGpkgFileNames(const QString& path);

	// 获取矢量分析就绪型数据包元数据信息
	bool GetMetaDataInVectorGpkg(string strGpkgFileFullPath, ARD_Package_MetaData_Info& info);

	// 获取栅格分析就绪型数据包元数据信息
	bool GetMetaDataInRasterGpkg(string strGpkgFileFullPath, ARD_Package_MetaData_Info& info);

	// 删除指定分析就绪数据对应的矢量图层
	bool DeleteVectorLayerByARDIdentify(string strGpkgFileFullName, string strARDIdentify);

	// 删除指定分析就绪数据对应的栅格表
	bool DeleteRasterByARDIdentify(string strGpkgFileFullName, string strARDIdentify);

};

#endif // SE_MANAGE_ARD_PACKAGE
