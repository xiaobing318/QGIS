#include "cse_dataprocessimp.h"
#include <math.h>

CSE_DataProcessImp::CSE_DataProcessImp()
{
}

CSE_DataProcessImp::~CSE_DataProcessImp()
{
}


// 创建属性字段
SE_Error CSE_DataProcessImp::SetFieldDefn(OGRLayer* poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth)
{
	for (int i = 0; i < fieldname.size(); i++)
	{
		//创建字段:字段+字段类型
		OGRFieldDefn Field(fieldname[i].c_str(), fieldtype[i]);	
		// 设置字段宽度，实际操作需要根据不同字段设置不同长度
		Field.SetWidth(fieldwidth[i]);		
		OGRErr err = poLayer->CreateField(&Field);
		if (err != OGRERR_NONE)
		{
			return SE_ERROR_CREATE_LAYER_FIELD_FAILED;
		}
	}
	return SE_ERROR_NONE;
}

// 获取点要素几何信息和属性信息
SE_Error CSE_DataProcessImp::Get_Point(
	OGRFeature* poFeature, 
	SE_DPoint& dPoint, 
	map<string, string>& mFieldValue)
{
	if (nullptr == poFeature) 
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}

	//将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	dPoint.dx = poPoint->getX();
	dPoint.dy = poPoint->getY();
	dPoint.dz = poPoint->getZ();
	
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));
	}

	return SE_ERROR_NONE;
}

// 获取点要素几何信息
SE_Error CSE_DataProcessImp::Get_Point(
	OGRFeature* poFeature,
	SE_DPoint& dPoint)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}

	//将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	dPoint.dx = poPoint->getX();
	dPoint.dy = poPoint->getY();
	dPoint.dz = poPoint->getZ();

	return SE_ERROR_NONE;
}


// 获取线要素几何信息和属性信息
SE_Error CSE_DataProcessImp::Get_LineString(
	OGRFeature* poFeature, 
	vector<SE_DPoint>& vPoint, 
	map<string, string>& mFieldValue)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}

	// 将几何结构转换成线类型
	OGRLineString* pLineGeo = (OGRLineString*)poGeometry;
	int pointnums = pLineGeo->getNumPoints();
	
	SE_DPoint dPoint;
	for (int i = 0; i < pointnums; i++)
	{
		dPoint.dx = pLineGeo->getX(i);
		dPoint.dy = pLineGeo->getY(i);
		dPoint.dz = pLineGeo->getZ(i);
		vPoint.push_back(dPoint);
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));
	}

	return SE_ERROR_NONE;
}

// 获取多边形几何信息和属性信息
SE_Error CSE_DataProcessImp::Get_Polygon(
	OGRFeature* poFeature, 
	vector<SE_DPoint>& ExteriorRing,
	vector<vector<SE_DPoint>>& InteriorRing,
	map<string, string>& mFieldValue)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	OGRGeometry* poGeometry = poFeature->GetGeometryRef();
	if (nullptr == poGeometry)
	{
		return SE_ERROR_FEATURE_GEOMETRY_IS_NULL;
	}

	//将几何结构转换成多边形类型
	OGRPolygon* poPolygon = (OGRPolygon*)poGeometry;
	OGRLinearRing* pOGRLinearRing = poPolygon->getExteriorRing();
	
	// 外环多边形
	if (nullptr == pOGRLinearRing)
	{
		return SE_ERROR_FEATURE_GEOMETRY_EXTERIOR_RING_IS_NULL;
	}


	//获取外环数据
	SE_DPoint dExteriorRingPoint;
	for (int i = 0; i < pOGRLinearRing->getNumPoints(); i++)
	{
		dExteriorRingPoint.dx = pOGRLinearRing->getX(i);
		dExteriorRingPoint.dy = pOGRLinearRing->getY(i);
		dExteriorRingPoint.dz = pOGRLinearRing->getZ(i);
		ExteriorRing.push_back(dExteriorRingPoint);
	}

	//获取内环数据（一个外环包含0个或多个内环）
	for (int i = 0; i < poPolygon->getNumInteriorRings(); i++)
	{
		vector<SE_DPoint> InterRingPoints;
		InterRingPoints.clear();

		pOGRLinearRing = poPolygon->getInteriorRing(i);
		for (int j = 0; j < pOGRLinearRing->getNumPoints(); j++)
		{
			SE_DPoint InterRingPoint;
			InterRingPoint.dx = pOGRLinearRing->getX(j);
			InterRingPoint.dy = pOGRLinearRing->getY(j);
			InterRingPoint.dz = pOGRLinearRing->getZ(j);
			InterRingPoints.push_back(InterRingPoint);
		}
		InteriorRing.push_back(InterRingPoints);
	}
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));
	}
	return 0;
}


// 设置点要素
SE_Error CSE_DataProcessImp::Set_Point(
	OGRLayer* poLayer,
	SE_DPoint dPoint,
	map<string, string> mFieldValue)
{
	// 图层为空
	if (nullptr == poLayer)
	{
		return SE_ERROR_OGRLAYER_IS_NULL;
	}

	OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 要素创建失败 
	if (nullptr == poFeature)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}


	// 根据提供的字段值map对相应字段赋值
	for (auto field : mFieldValue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
	}

	OGRPoint point;
	point.setX(dPoint.dx);
	point.setY(dPoint.dy);

	OGRErr oErr = poFeature->SetGeometry((OGRGeometry*)(&point));
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_SET_GEOMETRY_FAILED;
	}

	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poFeature);

	return SE_ERROR_NONE;
}

// 设置线要素
SE_Error CSE_DataProcessImp::Set_LineString(
	OGRLayer* poLayer,
	vector<SE_DPoint> Line,
	map<string, string> mFieldValue)
{
	// 图层为空
	if (nullptr == poLayer)
	{
		return SE_ERROR_OGRLAYER_IS_NULL;
	}

	OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 要素创建失败 
	if (nullptr == poFeature)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}

	// 根据提供的字段值map对相应字段赋值
	for (auto field : mFieldValue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
	}

	OGRLineString pLine;
	for (int i = 0; i < Line.size(); i++)
	{
		pLine.addPoint(Line[i].dx, Line[i].dy);
	}
		
	OGRErr oErr = poFeature->SetGeometry((OGRGeometry*)(&pLine));
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_SET_GEOMETRY_FAILED;
	}

	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poFeature);

	return SE_ERROR_NONE;
}


// 设置面要素
SE_Error CSE_DataProcessImp::Set_Polygon(
	OGRLayer* poLayer, 
	vector<SE_DPoint> ExteriorRing,
	vector<vector<SE_DPoint>> vInteriorRing,
	map<string, string> mFieldValue)
{

	// 图层为空
	if (nullptr == poLayer)
	{
		return SE_ERROR_OGRLAYER_IS_NULL;
	}

	OGRFeature* poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 要素创建失败 
	if (nullptr == poFeature)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}


	// 根据提供的字段值map对相应字段赋值
	for (auto field : mFieldValue)
	{
		poFeature->SetField(field.first.c_str(), field.second.c_str());
	}

	OGRPolygon polygon;
	// 外环
	OGRLinearRing ringOut;
	for (int i = 0; i < ExteriorRing.size(); i++)
	{
		ringOut.addPoint(ExteriorRing[i].dx, ExteriorRing[i].dy);
	}

	// 结束点应和起始点相同，保证多边形闭合
	ringOut.closeRings();
	
	OGRErr oErr;
	oErr = polygon.addRing(&ringOut); 
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_POLYGON_ADDRING_FAILED;
	}

	// 内环
	for (int i = 0; i < vInteriorRing.size(); i++)
	{
		OGRLinearRing ringIn;
		for (int j = 0; j < vInteriorRing[i].size(); j++)
		{
			ringIn.addPoint(vInteriorRing[i][j].dx, vInteriorRing[i][j].dy);
		}
		ringIn.closeRings();
		oErr = polygon.addRing(&ringIn);
		if (oErr != OGRERR_NONE)
		{
			return SE_ERROR_POLYGON_ADDRING_FAILED;
		}
	}


	oErr = poFeature->SetGeometry((OGRGeometry*)(&polygon));
	if (oErr != OGRERR_NONE)
	{
		return SE_ERROR_SET_GEOMETRY_FAILED;
	}

	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return SE_ERROR_CREATE_FEATURE_FAILED;
	}
	OGRFeature::DestroyFeature(poFeature);

	return SE_ERROR_NONE;
}

// 创建cpg文件
bool CSE_DataProcessImp::CreateShapefileCPG(const char* szCPGFilePath, const char* szEncoding)
{
	FILE* fp = fopen(szCPGFilePath, "w");
	if (!fp)
	{
		return false;
	}

	fprintf(fp, "%s", szEncoding);

	fclose(fp);

	return true;
}

// 获取POI级别配置信息
SE_Error CSE_DataProcessImp::Get_POI_Level_Info(OGRFeature* poFeature, POI_Level_Info& info)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		//（0）Level值
		if (iField == 0)
		{
			info.iLevel = poFeature->GetFieldAsInteger(iField);
		}

		//（1）Kind值
		else if (iField == 1)
		{
			info.iKind = poFeature->GetFieldAsInteger(iField);
		}

		//（2）字段名称1
		else if (iField == 2)
		{
			info.strFieldName1 = poFeature->GetFieldAsString(iField);
		}

		//（3）字段值1
		else if (iField == 3)
		{
			info.strFieldValueRule1 = poFeature->GetFieldAsString(iField);
		}

		//（4）字段名称2
		else if (iField == 4)
		{
			info.strFieldName2 = poFeature->GetFieldAsString(iField);
		}

		//（5）字段值2
		else if (iField == 5)
		{
			info.strFieldValueRule2 = poFeature->GetFieldAsString(iField);
		}

		//（6）SQL
		else if (iField == 6)
		{
			info.strSQL = poFeature->GetFieldAsString(iField);
		}
	}

	return SE_ERROR_NONE;
}

// 获取POI敏感地址配置信息
SE_Error CSE_DataProcessImp::Get_POI_SensitiveAddress(OGRFeature* poFeature, Sensitive_Address_Info& info)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		//（0）Level值
		if (iField == 0)
		{
			info.iLevel = poFeature->GetFieldAsInteger(iField);
		}

		//（1）Address
		else if (iField == 1)
		{
			info.strAddress = poFeature->GetFieldAsString(iField);
		}

	}

	return SE_ERROR_NONE;
}

// 获取POI敏感名称配置信息
SE_Error CSE_DataProcessImp::Get_POI_SensitiveName(OGRFeature* poFeature, Sensitive_Name_Info& info)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		//（0）Level值
		if (iField == 0)
		{
			info.iLevel = poFeature->GetFieldAsInteger(iField);
		}

		//（1）Name
		else if (iField == 1)
		{
			info.strName = poFeature->GetFieldAsString(iField);
		}

	}

	return SE_ERROR_NONE;
}


// 根据字段名获取字段值
void CSE_DataProcessImp::GetFieldValueByFieldName(map<string, string> mFieldValue, string strFieldName, string& strFieldValue)
{
	map<string, string>::iterator iter = mFieldValue.find(strFieldName);
	if (iter != mFieldValue.end())
	{
		strFieldValue = iter->second;
	}
	else
	{
		strFieldValue = "";
	}

}


// 根据字段名设置字段值
void CSE_DataProcessImp::SetFieldValueByFieldName(map<string, string>& mFieldValue, string strFieldName, string strFieldValue)
{
	mFieldValue[strFieldName] = strFieldValue;
}

// 根据格网宽度划分数据范围
void CSE_DataProcessImp::CalGridListByWidth(SE_DRect dDataExtent,
	vector<int> vLevelList,
	vector<double> vGridWidth,
	vector<vector<GridFeatureInfo>>& vGridFeatureInfoList)
{
	vGridFeatureInfoList.clear();

	for (int iGridIndex = 0; iGridIndex < vGridWidth.size(); iGridIndex++)
	{
		// 当前级别对应的格网集合
		vector<GridFeatureInfo> vGridInfo;
		vGridInfo.clear();

		// 当前级别格网宽度，单位：米
		double dWidth = vGridWidth[iGridIndex];

		// 当前格网级别
		int iCurrentLevel = vLevelList[iGridIndex];

		// 计算左边界
		int iLeftIndex = int(dDataExtent.dleft / dWidth);

		// 计算右边界
		int iRightIndex = ceil(dDataExtent.dright / dWidth);

		// 计算下边界
		int iBottomIndex = int(dDataExtent.dbottom / dWidth);

		// 计算上边界
		int iTopIndex = ceil(dDataExtent.dtop / dWidth);

		// 调整成整数边界
		dDataExtent.dleft = iLeftIndex * dWidth;
		dDataExtent.dright = iRightIndex * dWidth;
		dDataExtent.dtop = iTopIndex * dWidth;
		dDataExtent.dbottom = iBottomIndex * dWidth;

		int iColCount = iRightIndex - iLeftIndex;
		int iRowCount = iTopIndex - iBottomIndex;

		// 格网从下往上、从左往右依次进行计算
		GridFeatureInfo gridInfo;
		gridInfo.iLevel = iCurrentLevel;
		for (int i = 0; i < iRowCount; i++)
		{
			for (int j = 0; j < iColCount; j++)
			{
				gridInfo.dRect.dleft = dDataExtent.dleft + j * dWidth;
				gridInfo.dRect.dright = gridInfo.dRect.dleft + dWidth;
				gridInfo.dRect.dbottom = dDataExtent.dbottom + i * dWidth;
				gridInfo.dRect.dtop = gridInfo.dRect.dbottom + dWidth;

				vGridInfo.push_back(gridInfo);
			}
		}

		vGridFeatureInfoList.push_back(vGridInfo);
	}
}

/*品字形格网划分*/

// 根据格网宽度划分数据范围
void CSE_DataProcessImp::CalGridListByWidthPin(SE_DRect dDataExtent,
	vector<int> vLevelList,
	vector<double> vGridWidth,
	vector<vector<GridFeatureInfo>>& vGridFeatureInfoList)
{
	vGridFeatureInfoList.clear();

	for (int iGridIndex = 0; iGridIndex < vGridWidth.size(); iGridIndex++)
	{
		// 当前级别对应的格网集合
		vector<GridFeatureInfo> vGridInfo;
		vGridInfo.clear();

		// 当前级别格网宽度，单位：米
		double dWidth = vGridWidth[iGridIndex];

		// 当前格网级别
		int iCurrentLevel = vLevelList[iGridIndex];

		// 计算左边界
		int iLeftIndex = int(dDataExtent.dleft / dWidth);

		// 计算右边界
		int iRightIndex = ceil(dDataExtent.dright / dWidth);

		// 计算下边界
		int iBottomIndex = int(dDataExtent.dbottom / dWidth);

		// 计算上边界
		int iTopIndex = ceil(dDataExtent.dtop / dWidth);

		// 调整成整数边界
		dDataExtent.dleft = iLeftIndex * dWidth;
		dDataExtent.dright = iRightIndex * dWidth;
		dDataExtent.dtop = iTopIndex * dWidth;
		dDataExtent.dbottom = iBottomIndex * dWidth;

		int iColCount = iRightIndex - iLeftIndex;
		int iRowCount = iTopIndex - iBottomIndex;

		// 格网从下往上、从左往右依次进行计算
		GridFeatureInfo gridInfo;
		gridInfo.iLevel = iCurrentLevel;
		for (int i = 0; i < iRowCount; i++)
		{
			for (int j = 0; j < iColCount; j++)
			{
				// 如果是奇数行不需要调整左右边界，偶数行需要调整左右边界
				int flag = (i + 1) % 2;
				//	奇数行
				if(flag)
				{
					gridInfo.dRect.dleft = dDataExtent.dleft + j * dWidth;
					gridInfo.dRect.dright = gridInfo.dRect.dleft + dWidth;
					//	上下格网边界不需要调整
					gridInfo.dRect.dbottom = dDataExtent.dbottom + i * dWidth;
					gridInfo.dRect.dtop = gridInfo.dRect.dbottom + dWidth;

				}
				//	偶数行
				else
				{
					if((j != 0) && (j != (iColCount - 1)))
					{
						gridInfo.dRect.dleft = dDataExtent.dleft + ((double)j + 0.5) * dWidth;
						gridInfo.dRect.dright = gridInfo.dRect.dleft + dWidth;
						//	上下格网边界不需要调整
						gridInfo.dRect.dbottom = dDataExtent.dbottom + i * dWidth;
						gridInfo.dRect.dtop = gridInfo.dRect.dbottom + dWidth;			
					}
				}
				vGridInfo.push_back(gridInfo);
			}
		}

		vGridFeatureInfoList.push_back(vGridInfo);
	}
}


// 判断整数是否在列表中
bool CSE_DataProcessImp::IsInVectorList(int iValue, vector<int>& vIntValue)
{
	for (int i = 0; i < vIntValue.size(); i++)
	{
		if (iValue == vIntValue[i])
		{
			return true;
		}
	}

	return false;
}

// 判断整数是否在列表中
bool CSE_DataProcessImp::IsInGIntVectorList(GIntBig iValue, vector<GIntBig>& vIntValue)
{
	for (int i = 0; i < vIntValue.size(); i++)
	{
		if (iValue == vIntValue[i])
		{
			return true;
		}
	}

	return false;
}


// 根据Level值和坐标点计算所在级别格网索引
void CSE_DataProcessImp::CalLevelGridIndexByLevelAndPoint(
	int iLevel,
	SE_DPoint dPoint,
	vector<int>& vLevelList,
	vector<vector<GridFeatureInfo>>& vGridList,
	int& iLevelIndex,
	int& iGridIndex)
{
	iLevelIndex = -1;
	iGridIndex = -1;

	for (int i = 0; i < vLevelList.size(); i++)
	{
		// 查找相同级别
		if (vLevelList[i] == iLevel)
		{
			// 查找点所在的格网
			for (int j = 0; j < vGridList[i].size(); j++)
			{
				if (PointInRect(dPoint, vGridList[i][j].dRect))
				{
					iLevelIndex = i;
					iGridIndex = j;
				}
			}
		}
	}
}


// 判断点坐标是否在矩形内
bool CSE_DataProcessImp::PointInRect(SE_DPoint dPoint, SE_DRect dRect)
{
	if (dPoint.dx >= dRect.dleft
		&& dPoint.dx <= dRect.dright
		&& dPoint.dy >= dRect.dbottom
		&& dPoint.dy <= dRect.dtop)
	{
		return true;
	}
	return false;
}

// 计算格网内离格网中心点最近的点FID值
GIntBig CSE_DataProcessImp::CalNearestFIDInGrid(OGRLayer* poLayer,
	vector<GIntBig>& vFID,
	SE_DRect dRect,
	int iCurrentLevel,
	vector<int>& vLevelList,
	vector<GIntBig>& vLowerLevelFID)
{
	GIntBig iFID = -1;
	// 格网中心点坐标
	SE_DPoint dGridCenterPoint;
	dGridCenterPoint.dx = (dRect.dleft + dRect.dright) * 0.5;
	dGridCenterPoint.dy = (dRect.dbottom + dRect.dtop) * 0.5;

	// 最小距离
	double dMinDistance = DBL_MAX;
	
	double dDistance = 0;
	
	// 要素点坐标
	SE_DPoint dFeaturePoint;

	// 比当前级别高的FID集合
	vector<HighLevelFIDInfo> vHighLevelFID;
	vHighLevelFID.clear();


	for (int i = 0; i < vLevelList.size(); i++)
	{
		HighLevelFIDInfo info;

		if (vLevelList[i] < iCurrentLevel)
		{
			info.iLevel = vLevelList[i];
			vHighLevelFID.push_back(info);
		}
	}

	for (int i = 0; i < vFID.size(); i++)
	{

		// 获取要素
		OGRFeature* pFeature = poLayer->GetFeature(vFID[i]);

		map<string, string> mFieldValue;
		mFieldValue.clear();

		// 获取点几何信息和属性信息
		Get_Point(pFeature, dFeaturePoint, mFieldValue);

		// 获取级别属性值
		string strLevel;
		CSE_DataProcessImp::GetFieldValueByFieldName(mFieldValue, "Level", strLevel);

		int iLevel = atoi(strLevel.c_str());


		if (iLevel < iCurrentLevel)
		{
			for (int j = 0; j < vHighLevelFID.size(); j++)
			{
				if (iLevel == vHighLevelFID[j].iLevel)
				{
					vHighLevelFID[j].vFID.push_back(pFeature->GetFID());
				}
			}
		}


		OGRFeature::DestroyFeature(pFeature);
	}



	// 按照级别优先顺序，查找是否有高等级的点降到当前级别的点，
	// 如果存在，则从高等级里再查找距离中心点最近的，其他进行降级；
	// 如果不存在，则按照距离中心点最近的进行筛选，其他进行降级

	for (int i = 0; i < vHighLevelFID.size(); i++)
	{
		// 如果只有一个高等级点
		if (vHighLevelFID[i].vFID.size() == 1)
		{
			iFID = vHighLevelFID[i].vFID[0];
			break;
		}
		// 如果有两个及以上高等级点，则按照距离格网中心近的优先
		else if(vHighLevelFID[i].vFID.size() > 1)
		{
			// 则从当前级别列表中查找距离格网中心最近的FID
			for (int j = 0; j < vHighLevelFID[i].vFID.size(); j++)
			{

				// 获取要素
				OGRFeature* pFeature = poLayer->GetFeature(vHighLevelFID[i].vFID[j]);


				// 获取点几何信息和属性信息
				Get_Point(pFeature, dFeaturePoint);

				dDistance = pow((dFeaturePoint.dx - dGridCenterPoint.dx), 2) + pow((dFeaturePoint.dy - dGridCenterPoint.dy), 2);
				if (dDistance < dMinDistance)
				{
					dMinDistance = dDistance;
					iFID = vFID[i];
				}

				OGRFeature::DestroyFeature(pFeature);
			}
			break;
		}
	}

	// 如果没有高等级点
	if (iFID == -1)
	{
		// 则从当前级别列表中查找距离格网中心最近的FID
		for (int i = 0; i < vFID.size(); i++)
		{

			// 获取要素
			OGRFeature* pFeature = poLayer->GetFeature(vFID[i]);


			// 获取点几何信息和属性信息
			Get_Point(pFeature, dFeaturePoint);

			dDistance = sqrt(pow((dFeaturePoint.dx - dGridCenterPoint.dx), 2) + pow((dFeaturePoint.dy - dGridCenterPoint.dy), 2));
			if (dDistance < dMinDistance)
			{
				dMinDistance = dDistance;
				iFID = vFID[i];
			}

			OGRFeature::DestroyFeature(pFeature);
		}

	}

	vLowerLevelFID.clear();
	// 返回需降级的FID集合
	for (int i = 0; i < vFID.size(); i++)
	{
		if (iFID != vFID[i])
		{
			vLowerLevelFID.push_back(vFID[i]);
		}
	}

	return iFID;
}

SE_Error CSE_DataProcessImp::Get_Attribute(OGRFeature* poFeature, map<string, string>& mFieldValue)
{
	if (nullptr == poFeature)
	{
		return SE_ERROR_FEATURE_IS_NULL;
	}

	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		if (nullptr == pField)
		{
			return SE_ERROR_FIELDDEFN_IS_NULL;
		}

		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);
		mFieldValue.insert(map<string, string>::value_type(strFieldName, strValue));
	}

	return SE_ERROR_NONE;
}

void CSE_DataProcessImp::GetFIDsByRelType(POI_Parenthood_Info& info, int iRel_Type, vector<GIntBig>& vFIDs)
{
	vFIDs.clear();

	for (int i = 0; i < info.vRel_Type.size(); i++)
	{
		if (info.vRel_Type[i] == iRel_Type)
		{
			vFIDs.push_back(info.vChildrenFID[i]);
		}
	}
}

int CSE_DataProcessImp::GetMax(int iValue1, int iValue2)
{
	if (iValue1 <= iValue2)
	{
		return iValue2;
	}
	else
	{
		return iValue1;
	}
}

int CSE_DataProcessImp::GetMin(int iValue1, int iValue2)
{
	if (iValue1 <= iValue2)
	{
		return iValue1;
	}
	else
	{
		return iValue2;
	}
}

void CSE_DataProcessImp::CalGridIndexListByLevelAndPoint(
	int iLevel, 
	SE_DPoint dPoint, 
	vector<int>& vLevelList, 
	vector<vector<GridFeatureInfo>>& vGridList, 
	vector<int>& vLowerLevelList, 
	vector<int>& vGridIndexList)
{
	vLowerLevelList.clear();
	vGridIndexList.clear();

	// 循环处理每个级别
	for (int i = iLevel; i <= vLevelList.back(); i++)
	{
		int iLevelIndex = 0;
		int iGridIndex = 0;

		CalLevelGridIndexByLevelAndPoint(i, dPoint, vLevelList, vGridList, iLevelIndex, iGridIndex);

		vLowerLevelList.push_back(iLevelIndex);
		vGridIndexList.push_back(iGridIndex);
	}
}

bool CSE_DataProcessImp::bMapIsEqual(MAP_STRING_2_STRING& map1, MAP_STRING_2_STRING& map2)
{
	if (map1.size() != map2.size())
	{
		return false;
	}

	auto iter1 = map1.begin();
	auto iter2 = map2.begin();
	for (; iter1 != map1.end(); ++iter1, ++iter2)
	{
		if (iter1->first != iter2->first || iter1->second != iter2->second)
		{
			return false;
		}
	}

	return true;
}


bool CSE_DataProcessImp::RoadIntersectLrrlAndSubL(
	OGRFeature* pRoadFeature,
	OGRLayer* pLrrlLayer,
	OGRLayer* pSublLayer)
{
	// 获取道路要素的最小外接矩形
	OGREnvelope oRoadEnvelop;
	OGRGeometry* pRoadGeometry = pRoadFeature->GetGeometryRef();
	pRoadGeometry->getEnvelope(&oRoadEnvelop);

	// 设置铁路图层的空间查询条件
	pLrrlLayer->SetSpatialFilterRect(oRoadEnvelop.MinX,
		oRoadEnvelop.MinY,
		oRoadEnvelop.MaxX,
		oRoadEnvelop.MaxY);

	pLrrlLayer->ResetReading();
	OGRFeature* pLrrlFeature = nullptr;
	while ((pLrrlFeature = pLrrlLayer->GetNextFeature()) != nullptr)
	{
		OGRGeometry* pLrrlGeometry = pLrrlFeature->GetGeometryRef();
		OGRGeometry* pIntersectGeo =  pRoadGeometry->Intersection(pLrrlGeometry);
		// 只要有一个相交，则直接返回true
		if (pIntersectGeo)
		{
			return true;
		}

		OGRFeature::DestroyFeature(pLrrlFeature);
	}
	
	// 设置地铁图层的空间查询条件
	pSublLayer->SetSpatialFilterRect(oRoadEnvelop.MinX,
		oRoadEnvelop.MinY,
		oRoadEnvelop.MaxX,
		oRoadEnvelop.MaxY);

	pSublLayer->ResetReading();
	OGRFeature* pSublFeature = nullptr;
	while ((pSublFeature = pSublLayer->GetNextFeature()) != nullptr)
	{
		OGRGeometry* pSublGeometry = pSublFeature->GetGeometryRef();
		OGRGeometry* pIntersectGeo = pRoadGeometry->Intersection(pSublGeometry);
		// 只要有一个相交，则直接返回true
		if (pIntersectGeo)
		{
			return true;
		}

		OGRFeature::DestroyFeature(pSublFeature);
	}

	return false;
}



GIntBig CSE_DataProcessImp::FindFidByAttr(SE_DPoint dPoiPoint,string strValue, map<string, LayerPointInfo>& mapFeatureInfo, double dBufferDistance)
{
	map<string, LayerPointInfo>::iterator iter = mapFeatureInfo.find(strValue);

	// 如果名称相同
	if (iter != mapFeatureInfo.end())
	{
		double dDistance = sqrt(pow(dPoiPoint.dx - iter->second.dPoint.dx, 2) + pow(dPoiPoint.dy - iter->second.dPoint.dy, 2)) * 111.0;			// 单位：千米
	
		if (dDistance <= dBufferDistance)
		{
			return iter->second.iFID;
		}
	
	}

	return -1;

}

