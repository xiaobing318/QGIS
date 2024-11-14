#include "convert_data_to_gb_or_gjb.h"

#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QtXml/qdom.h>

/*int  GeoExtractAndProcessProgress(
	double dTotalWork,
	double dCompletedWork,
	double dPercent,
	const char* pszMessage)
{
	//ui.progressBar->setMaximum(dTotalWork);
	//ui.progressBar->setValue(dCompletedWork);
	return 1;
}*/

QgsConvertDataToGB_OR_GJB::QgsConvertDataToGB_OR_GJB(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.Button_Save, &QPushButton::clicked, this, &QgsConvertDataToGB_OR_GJB::Button_Save_clicked);
	connect(ui.Button_Open, &QPushButton::clicked, this, &QgsConvertDataToGB_OR_GJB::Button_Open_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsConvertDataToGB_OR_GJB::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsConvertDataToGB_OR_GJB::Button_Cancel_rejected);

	restoreState();
	ui.lineEdit_InputDataPath->setText(m_qstrInputDataPath);
	ui.lineEdit_SaveDataPath->setText(m_qstrSaveDataPath);

	// 默认状态选择从文件获取范围
	ui.radioButton_MinKou->setChecked(true);
}

QgsConvertDataToGB_OR_GJB::~QgsConvertDataToGB_OR_GJB()
{
	// 设置当前保存路径
	QgsSettings settings;
	settings.setValue(QStringLiteral("ConvertDataToGB_OR_GJB/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("ConvertDataToGB_OR_GJB/SaveDataPath"), m_qstrSaveDataPath, QgsSettings::Section::Plugins);
}

QString QgsConvertDataToGB_OR_GJB::GetLayerSavePath()
{
	return m_qstrSaveDataPath;
}

QString QgsConvertDataToGB_OR_GJB::GetInputFilePath()
{
	return m_qstrInputDataPath;
}

int QgsConvertDataToGB_OR_GJB::GetOutputType()
{
	// 0：国标数据；1：军标数据
	if (ui.radioButton_MinKou->isChecked())
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

void QgsConvertDataToGB_OR_GJB::Button_Save_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrSaveDataPath;
	QString dlgTile = QObject::tr("请选择国军标数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_SaveDataPath->setText(selectedDir);
		m_qstrSaveDataPath = selectedDir;
	}
}

void QgsConvertDataToGB_OR_GJB::Button_Open_clicked()
{
	// 选择文件夹
	QString curPath = m_qstrInputDataPath;
	QString dlgTile = QObject::tr("请选择输入数据路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_InputDataPath->setText(selectedDir);
		m_qstrInputDataPath = selectedDir;
	}
}

/*获取在指定目录下的目录的路径*/
QStringList QgsConvertDataToGB_OR_GJB::GetDirPathOfSplDir(QString dirPath)
{
	QStringList dirPaths;
	QDir splDir(dirPath);
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	QFileInfo tempFileInfo;
	foreach(tempFileInfo, fileInfoListInSplDir) {
		dirPaths << tempFileInfo.absoluteFilePath();
	}
	return dirPaths;
}

void QgsConvertDataToGB_OR_GJB::Button_OK_accepted()
{
	if (!FilePathIsExisted(ui.lineEdit_InputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("输入数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(ui.lineEdit_SaveDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("输出数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 调用GJB数据导出接口进行处理
	// 获取数据来源
	int iDataType = GetOutputType();

	// 获取输入数据路径
	QString qstrInputDataPath = GetInputFilePath();
	string strInputDataPath = qstrInputDataPath.toLocal8Bit();

	// 获取数据保存目录
	QString qstrSaveDataPath = GetLayerSavePath();
	string strSaveDataPath = qstrSaveDataPath.toLocal8Bit();

	// 获取图幅列表
	QStringList qstrPathList = GetDirPathOfSplDir(qstrInputDataPath);

	// 读取配置文件
	// 国标图层配置文件
	vector<GBLayerInfo> vGBLayerInfo;
	vGBLayerInfo.clear();
	LoadBetterThan5wConfig_GBLayer(vGBLayerInfo);

	// 读取配置文件
	// 国军标图层配置文件
	vector<GJBLayerInfo> vGJBLayerInfo;
	vGJBLayerInfo.clear();
	LoadBetterThan5wConfig_GJBLayerField(vGJBLayerInfo);

	// 读取GB到GJB编码配置文件
	vector<GB2GJB_FeatureClassMap> vGB2GJB;
	vGB2GJB.clear();
	LoadBetterThan5wConfig_GBCode2GJBCode(vGB2GJB);

	// 读取JB到GJB编码配置文件
	vector<JB2GJB_FeatureClassMap> vJB2GJB;
	vJB2GJB.clear();
	LoadBetterThan5wConfig_JBCode2GJBCode(vJB2GJB);


	// 国军标一体化图层名称列表
	vector<string> vGJBLayerName;
	vGJBLayerName.clear();
	for (int i = 0; i < vGJBLayerInfo.size(); i++)
	{
		vGJBLayerName.push_back(vGJBLayerInfo[i].strName);
	}

	// 军标图层配置文件
	vector<JBLayerInfo> vJBLayerInfo;
	vJBLayerInfo.clear();
	LoadBetterThan5wConfig_JBLayer(vJBLayerInfo);

	// 国标图层字段列表配置文件
	vector<GB_LayerFieldList> vGB_LayerFieldList;
	vGB_LayerFieldList.clear();
	LoadBetterThan5wConfig_GBLayerFieldList(vGB_LayerFieldList);

	// 军标图层字段列表配置文件
	vector<JB_LayerFieldList> vJB_LayerFieldList;
	vJB_LayerFieldList.clear();
	LoadBetterThan5wConfig_JBLayerFieldList(vJB_LayerFieldList);

	vector<string> vInputPath;
	vInputPath.clear();

#pragma region "判断输入的GJB数据是否符合规范"


	// 如果是单个图幅
	if (qstrPathList.size() == 0)
	{
		// 获取图幅下所有shp文件列表
		QStringList qShpFileList = GetShpFileNames(qstrInputDataPath);

		if (qShpFileList.size() == 0)
		{
			// 记录日志
			QMessageBox msgBox1;
			msgBox1.setWindowTitle(tr("地理提取与加工"));
			msgBox1.setText(tr("输入的文件夹下无shp文件！"));
			msgBox1.setStandardButtons(QMessageBox::Yes);
			msgBox1.setDefaultButton(QMessageBox::Yes);
			msgBox1.exec();
			return;
		}

		for (int i = 0; i < qShpFileList.size(); i++)
		{
			string strShpFileName = qShpFileList[i].toLocal8Bit();

			// 只要有一个文件不合法，说明输入文件不符合要求，提示并退出操作
			if (!bIsExistedInLayerNameList(strShpFileName, vGJBLayerName))
			{
				// 跳过当前文件
				continue;
			}
		}
	}
	else
	{
		for (int i = 0; i < qstrPathList.size(); i++)
		{
			// 判断每个文件夹下数据是否符合标准
			// 获取图幅下所有shp文件列表
			QStringList qShpFileList = GetShpFileNames(qstrPathList[i]);

			if (qShpFileList.size() == 0)
			{
				// 记录日志
				QMessageBox msgBox1;
				msgBox1.setWindowTitle(tr("地理提取与加工"));
				msgBox1.setText(tr("输入的文件夹下无shp文件！"));
				msgBox1.setStandardButtons(QMessageBox::Yes);
				msgBox1.setDefaultButton(QMessageBox::Yes);
				msgBox1.exec();
				return;
			}

			for (int j = 0; j < qShpFileList.size(); j++)
			{
				string strShpFileName = qShpFileList[j].toLocal8Bit();

				// 只要有一个文件不合法，说明输入文件不符合要求，提示并退出操作
				if (!bIsExistedInLayerNameList(strShpFileName, vGJBLayerName))
				{
					// 跳过当前文件
					continue;
				}
			}
		}
	}

#pragma endregion



	// 如果是单图幅
	if (qstrPathList.size() == 0)
	{
		vInputPath.push_back(strInputDataPath);
	}
	else
	{
		for (int i = 0; i < qstrPathList.size(); i++)
		{
			string strPathTemp = qstrPathList[i].toLocal8Bit();
			// 调用国军标一体化数据导出算法
			vInputPath.push_back(strPathTemp);
		}
	}

	int iResult = 0;
	// 调用国军标一体化数据导出算法
	iResult = CSE_GeoExtractAndProcess::ConvertDataToGB_OR_JB(vInputPath,
		strSaveDataPath,
		iDataType,
		vGJBLayerInfo,
		vGBLayerInfo,
		vJBLayerInfo,
		vGB2GJB,
		vJB2GJB,
		vGB_LayerFieldList,
		vJB_LayerFieldList,
		nullptr);

	if (iResult != 0)
	{
		// 记录日志
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("地理提取与加工"));
		msgBox1.setText(tr("国/军标数据导出失败！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}
	else
	{
		// 设置当前保存路径
		QgsSettings settings;
		settings.setValue(QStringLiteral("ConvertDataToGB_OR_GJB/InputDataPath"), m_qstrInputDataPath, QgsSettings::Section::Plugins);
		settings.setValue(QStringLiteral("ConvertDataToGB_OR_GJB/SaveDataPath"), m_qstrSaveDataPath, QgsSettings::Section::Plugins);

		// 记录日志
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("地理提取与加工"));
		msgBox1.setText(tr("国/军标数据导出完成！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}
}

void QgsConvertDataToGB_OR_GJB::Button_Cancel_rejected()
{
	reject();
}

void QgsConvertDataToGB_OR_GJB::restoreState()
{
	const QgsSettings settings;
	m_qstrInputDataPath = settings.value(QStringLiteral("ConvertDataToGB_OR_GJB/InputDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrSaveDataPath = settings.value(QStringLiteral("ConvertDataToGB_OR_GJB/SaveDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
}

void QgsConvertDataToGB_OR_GJB::LoadBetterThan5wConfig_GBLayer(vector<GBLayerInfo>& vGBLayerInfo)
{
	// 配置文件默认在运行目录下
	vGBLayerInfo.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/GB_Layer.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 循环遍历每个子节点
		GBLayerInfo gbLayerInfo;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					gbLayerInfo.strName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "LayerNameAlias")
				{
					gbLayerInfo.strAlias = elementTemp.text().toLocal8Bit();
				}

				if (elementTemp.tagName() == "LayerGeoType")
				{
					gbLayerInfo.strGeoType = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "GJBLayerList")
				{
					// 获取GJB图层列表
					QDomNodeList gjbLayerList = nodeTemp.childNodes();
					for (int j = 0; j < gjbLayerList.size(); j++)
					{
						QDomNode gjbNode = gjbLayerList.at(j);
						QDomElement gjbElement = gjbNode.toElement();
						QString qstrGJB = gjbElement.text();
						string strGJBLayer = qstrGJB.toLocal8Bit();
						gbLayerInfo.vGJBLayer.push_back(strGJBLayer);
					}
				}
			}
		}

		vGBLayerInfo.push_back(gbLayerInfo);

		node = node.nextSibling();//读取下一个兄弟节点
	}
}

void QgsConvertDataToGB_OR_GJB::LoadBetterThan5wConfig_JBLayer(vector<JBLayerInfo>& vJBLayerInfo)
{
	// 配置文件默认在运行环境下
	vJBLayerInfo.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/JB_Layer.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 循环遍历每个子节点
		JBLayerInfo jbLayerInfo;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					jbLayerInfo.strName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "LayerNameAlias")
				{
					jbLayerInfo.strAlias = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "GJBLayerList")
				{
					// 获取GJB图层列表
					QDomNodeList gjbLayerList = nodeTemp.childNodes();
					for (int j = 0; j < gjbLayerList.size(); j++)
					{
						QDomNode gjbNode = gjbLayerList.at(j);
						QDomElement gjbElement = gjbNode.toElement();
						QString qstrGJB = gjbElement.text();
						string strGJBLayer = qstrGJB.toLocal8Bit();
						jbLayerInfo.vGJBLayer.push_back(strGJBLayer);
					}
				}
			}
		}

		vJBLayerInfo.push_back(jbLayerInfo);

		node = node.nextSibling();//读取下一个兄弟节点
	}
}

void QgsConvertDataToGB_OR_GJB::LoadBetterThan5wConfig_GJBLayerField(vector<GJBLayerInfo>& vGJBLayerInfo)
{
	// 配置文件默认在运行目录下
	vGJBLayerInfo.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/GJB_LayerField.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			// 循环遍历每个子节点
			GJBLayerInfo gjbLayerInfo;
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					gjbLayerInfo.strName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "LayerNameAlias")
				{
					gjbLayerInfo.strAlias = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "Field_List")
				{
					// 获取GJB图层字段列表
					QDomNodeList gjbFieldList = nodeTemp.childNodes();
					for (int j = 0; j < gjbFieldList.size(); j++)
					{
						// Field节点
						QDomNode gjbFieldNode = gjbFieldList.at(j);

						// 获取Field节点的所有子节点
						QDomNodeList fieldNodeList = gjbFieldNode.childNodes();
						GJBFieldInfo fieldInfo;
						for (int k = 0; k < fieldNodeList.size(); k++)
						{
							QDomNode fieldInfoNode = fieldNodeList.at(k);
							QDomElement elementfieldInfo = fieldInfoNode.toElement();

							// 依次判断字段节点下所有节点的名称
							if (elementfieldInfo.tagName() == "Name")
							{
								fieldInfo.strName = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Alias")
							{
								fieldInfo.strAlias = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Must")
							{
								fieldInfo.strMust = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Default")
							{
								fieldInfo.strDefault = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Type")
							{
								fieldInfo.strType = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Len")
							{
								fieldInfo.iLength = elementfieldInfo.text().toLocal8Bit().toInt();
							}

							else if (elementfieldInfo.tagName() == "Unit")
							{
								fieldInfo.strUnit = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "ValidValues")
							{
								fieldInfo.strValidValues = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Detail")
							{
								fieldInfo.strDetail = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "KeyField")
							{
								fieldInfo.strKeyField = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "DataSource_List")
							{
								// 获取DataSource_List节点的所有子节点
								QDomNodeList dataSourceNodeList = fieldInfoNode.childNodes();
								for (int m = 0; m < dataSourceNodeList.size(); m++)
								{
									QDomNode dataSourceNode = dataSourceNodeList.at(m);

									// 获取DataSource节点下所有节点
									QDomNodeList dataSourceTempList = dataSourceNode.childNodes();

									for (int n = 0; n < dataSourceTempList.size(); n++)
									{
										QDomNode dataSourceTemp = dataSourceTempList.at(n);
										QDomElement dataSourceInfo = dataSourceTemp.toElement();

										if (dataSourceInfo.tagName() == "DataSourceName")
										{
											string strDataSourceName = dataSourceInfo.text().toLocal8Bit();
											fieldInfo.vDataSourceName.push_back(strDataSourceName);
										}

										else if (dataSourceInfo.tagName() == "DataSourceLayerName")
										{
											string strDataSourceLayerName = dataSourceInfo.text().toLocal8Bit();
											fieldInfo.vDataSourceLayerName.push_back(strDataSourceLayerName);
										}

										else if (dataSourceInfo.tagName() == "DataSourceField")
										{
											string strDataSourceField = dataSourceInfo.text().toLocal8Bit();
											fieldInfo.vDataSourceField.push_back(strDataSourceField);
										}
									}
								}
							}
						}
						gjbLayerInfo.vFields.push_back(fieldInfo);
					}
				}
			}

			vGJBLayerInfo.push_back(gjbLayerInfo);
		}
		node = node.nextSibling();//读取下一个兄弟节点
	}
}

/*加载GB图层字段列表*/
void QgsConvertDataToGB_OR_GJB::LoadBetterThan5wConfig_GBLayerFieldList(vector<GB_LayerFieldList>& vGBLayerFieldList)
{
	// 配置文件默认在运行目录下
	vGBLayerFieldList.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/GB_LayerField.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			// 循环遍历每个子节点
			GB_LayerFieldList gbLayerFieldList;
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					gbLayerFieldList.strName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "LayerNameAlias")
				{
					gbLayerFieldList.strAlias = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "Field_List")
				{
					// 获取GB图层字段列表
					QDomNodeList gbFieldList = nodeTemp.childNodes();
					for (int j = 0; j < gbFieldList.size(); j++)
					{
						// Field节点
						QDomNode gbFieldNode = gbFieldList.at(j);

						// 获取Field节点的所有子节点
						QDomNodeList fieldNodeList = gbFieldNode.childNodes();
						GB_FieldInfo fieldInfo;
						for (int k = 0; k < fieldNodeList.size(); k++)
						{
							QDomNode fieldInfoNode = fieldNodeList.at(k);
							QDomElement elementfieldInfo = fieldInfoNode.toElement();

							// 依次判断字段节点下所有节点的名称
							if (elementfieldInfo.tagName() == "Name")
							{
								fieldInfo.strName = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Alias")
							{
								fieldInfo.strAlias = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Must")
							{
								fieldInfo.strMust = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Default")
							{
								fieldInfo.strDefault = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Type")
							{
								fieldInfo.strType = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Len")
							{
								fieldInfo.iLength = elementfieldInfo.text().toLocal8Bit().toInt();
							}

							else if (elementfieldInfo.tagName() == "Unit")
							{
								fieldInfo.strUnit = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "ValidValues")
							{
								fieldInfo.strValidValues = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Detail")
							{
								fieldInfo.strDetail = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "KeyField")
							{
								fieldInfo.strKeyField = elementfieldInfo.text().toLocal8Bit();
							}
						}
						gbLayerFieldList.vFields.push_back(fieldInfo);
					}
				}
			}

			vGBLayerFieldList.push_back(gbLayerFieldList);
		}
		node = node.nextSibling();//读取下一个兄弟节点
	}
}

/*加载JB图层字段列表*/
void QgsConvertDataToGB_OR_GJB::LoadBetterThan5wConfig_JBLayerFieldList(vector<JB_LayerFieldList>& vJBLayerFieldList)
{
	// 配置文件默认在运行目录下
	vJBLayerFieldList.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/JB_LayerField.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			// 循环遍历每个子节点
			JB_LayerFieldList jbLayerFieldList;
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					jbLayerFieldList.strName = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "LayerNameAlias")
				{
					jbLayerFieldList.strAlias = elementTemp.text().toLocal8Bit();
				}

				else if (elementTemp.tagName() == "Field_List")
				{
					// 获取GB图层字段列表
					QDomNodeList gbFieldList = nodeTemp.childNodes();
					for (int j = 0; j < gbFieldList.size(); j++)
					{
						// Field节点
						QDomNode gbFieldNode = gbFieldList.at(j);

						// 获取Field节点的所有子节点
						QDomNodeList fieldNodeList = gbFieldNode.childNodes();
						JB_FieldInfo fieldInfo;
						for (int k = 0; k < fieldNodeList.size(); k++)
						{
							QDomNode fieldInfoNode = fieldNodeList.at(k);
							QDomElement elementfieldInfo = fieldInfoNode.toElement();

							// 依次判断字段节点下所有节点的名称
							if (elementfieldInfo.tagName() == "Name")
							{
								fieldInfo.strName = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Alias")
							{
								fieldInfo.strAlias = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Must")
							{
								fieldInfo.strMust = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Default")
							{
								fieldInfo.strDefault = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Type")
							{
								fieldInfo.strType = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Len")
							{
								fieldInfo.iLength = elementfieldInfo.text().toLocal8Bit().toInt();
							}

							else if (elementfieldInfo.tagName() == "Unit")
							{
								fieldInfo.strUnit = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "ValidValues")
							{
								fieldInfo.strValidValues = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "Detail")
							{
								fieldInfo.strDetail = elementfieldInfo.text().toLocal8Bit();
							}

							else if (elementfieldInfo.tagName() == "KeyField")
							{
								fieldInfo.strKeyField = elementfieldInfo.text().toLocal8Bit();
							}
						}
						jbLayerFieldList.vFields.push_back(fieldInfo);
					}
				}
			}

			vJBLayerFieldList.push_back(jbLayerFieldList);
		}
		node = node.nextSibling();//读取下一个兄弟节点
	}
}




QStringList QgsConvertDataToGB_OR_GJB::GetShpFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.shp" << "*.SHP";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

bool QgsConvertDataToGB_OR_GJB::bIsExistedInLayerNameList(string strLayerName, vector<string>& vNameList)
{
	int iCount = 0;
	for (int i = 0; i < vNameList.size(); i++)
	{
		if (strstr(strLayerName.c_str(), vNameList[i].c_str()) != NULL)
		{
			iCount++;
		}
	}

	// 说明未找到
	if (iCount == 0)
	{
		return false;
	}
	else
	{
		return true;
	}

	return true;
}


// 判断目录是否存在
bool QgsConvertDataToGB_OR_GJB::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

void QgsConvertDataToGB_OR_GJB::LoadBetterThan5wConfig_GBCode2GJBCode(vector<GB2GJB_FeatureClassMap>& vGB2GJB)
{
	// 配置文件默认在运行目录下
	vGB2GJB.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/GBCode2GJBCode.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 循环遍历每个子节点
		GB2GJB_FeatureClassMap gb2gjb;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "GBLayerName")
				{
					QByteArray qByte = elementTemp.text().toLocal8Bit();
					gb2gjb.strGBName = string(qByte);
				}

				else if (elementTemp.tagName() == "GBLayerNameAlias")
				{
					QByteArray qByte = elementTemp.text().toLocal8Bit();
					gb2gjb.strGBAlias = string(qByte);
				}

				else if (elementTemp.tagName() == "GJBLayerName")
				{
					QByteArray qByte = elementTemp.text().toLocal8Bit();
					gb2gjb.strGJBName = string(qByte);
				}

				else if (elementTemp.tagName() == "GJBLayerNameAlias")
				{
					QByteArray qByte = elementTemp.text().toLocal8Bit();
					gb2gjb.strGJBAlias = string(qByte);
				}

				else if (elementTemp.tagName() == "Codes")
				{
					QDomNodeList codeList = nodeTemp.childNodes();
					for (int j = 0; j < codeList.size(); j++)
					{
						QDomNode codeNode = codeList.at(j);

						QDomNodeList codeMapList = codeNode.childNodes();

						// GBCode结点
						QDomNode GBCode = codeMapList.at(0);

						// GJBCode结点
						QDomNode GJBCode = codeMapList.at(1);


						QDomElement GBCodeElement = GBCode.toElement();
						QDomElement GJBCodeElement = GJBCode.toElement();


						gb2gjb.vGBCode.push_back(string(GBCodeElement.text().toLocal8Bit()));
						gb2gjb.vGJBCode.push_back(string(GJBCodeElement.text().toLocal8Bit()));
					}
				}
			}
		}

		vGB2GJB.push_back(gb2gjb);

		node = node.nextSibling();//读取下一个兄弟节点
	}
}


void QgsConvertDataToGB_OR_GJB::LoadBetterThan5wConfig_JBCode2GJBCode(vector<JB2GJB_FeatureClassMap>& vJB2GJB)
{
	// 配置文件默认在运行目录下
	vJB2GJB.clear();
	QString curPath = QCoreApplication::applicationDirPath();
	QString strFileName = curPath + "/config/better_than_5w_xml/JBCode2GJBCode.xml";

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		// 循环遍历每个子节点
		JB2GJB_FeatureClassMap jb2gjb;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "JBLayerName")
				{
					QByteArray qByte = elementTemp.text().toLocal8Bit();
					jb2gjb.strJBName = string(qByte);
				}

				else if (elementTemp.tagName() == "JBLayerNameAlias")
				{
					QByteArray qByte = elementTemp.text().toLocal8Bit();
					jb2gjb.strJBAlias = string(qByte);
				}

				else if (elementTemp.tagName() == "GJBLayerName")
				{
					QByteArray qByte = elementTemp.text().toLocal8Bit();
					jb2gjb.strGJBName = string(qByte);
				}

				else if (elementTemp.tagName() == "GJBLayerNameAlias")
				{
					QByteArray qByte = elementTemp.text().toLocal8Bit();
					jb2gjb.strGJBAlias = string(qByte);
				}

				else if (elementTemp.tagName() == "Codes")
				{
					QDomNodeList codeList = nodeTemp.childNodes();
					for (int j = 0; j < codeList.size(); j++)
					{
						QDomNode codeNode = codeList.at(j);

						QDomNodeList codeMapList = codeNode.childNodes();

						JBMatchField matchField;
						// 遍历<Code>结点下的元素
						for (int k = 0; k < codeMapList.size(); ++k)
						{

							QDomNode nodeCodeChildren = codeMapList.at(k);
							QDomElement elementCodeChildren = nodeCodeChildren.toElement();
							// JB编码
							if (elementCodeChildren.tagName() == "JBCode")
							{
								QByteArray qByte = elementCodeChildren.text().toLocal8Bit();
								matchField.strJBCode = string(qByte);
							}

							// GJB编码
							else if (elementCodeChildren.tagName() == "GJBCode")
							{
								QByteArray qByte = elementCodeChildren.text().toLocal8Bit();
								matchField.strGJBCode = string(qByte);
							}

							// 等级
							else if (elementCodeChildren.tagName() == "DENGJI")
							{
								QByteArray qByte = elementCodeChildren.text().toLocal8Bit();
								matchField.strDengJi = string(qByte);
							}

							// 类型
							else if (elementCodeChildren.tagName() == "LEIXING")
							{
								QByteArray qByte = elementCodeChildren.text().toLocal8Bit();
								matchField.strLeiXing = string(qByte);
							}

							// 类型相等标志
							else if (elementCodeChildren.tagName() == "LEIXING_EQUAL")
							{
								QByteArray qByte = elementCodeChildren.text().toLocal8Bit();
								string strEqual = string(qByte);

								matchField.iLeiXingEqualFlag = atoi(strEqual.c_str());
							}

							// 图形特征
							else if (elementCodeChildren.tagName() == "TUXINGTEZHENG")
							{
								QByteArray qByte = elementCodeChildren.text().toLocal8Bit();
								string strTuXingTeZheng = string(qByte);

								matchField.strTuXingTeZheng = atoi(strTuXingTeZheng.c_str());
							}

						}

						jb2gjb.vMatchFieldList.push_back(matchField);


					}
				}
			}
		}

		vJB2GJB.push_back(jb2gjb);

		node = node.nextSibling();//读取下一个兄弟节点
	}
}
