#include "cse_mapsheet.h"
#define DOUBLE_EQUAL_ZERO 1e-6

CSE_MapSheet::CSE_MapSheet()
{

}

CSE_MapSheet::~CSE_MapSheet()
{

}

/*根据经纬度范围(bbox)获取某比例尺图幅列表*/
void CSE_MapSheet::get_name_from_bbox(double scale,
	string map_type,
	double lon_min,
	double lat_min,
	double lon_max,
	double lat_max,
	vector<string>& vTileCodes)
{
	vTileCodes.clear();

	vector<string> vTileInnerCodes;
	vTileInnerCodes.clear();

	vector<string> vTileOuterCodes;
	vTileOuterCodes.clear();

	_get_name_list(scale, map_type, lon_min, lat_min, lon_max, lat_max, vTileInnerCodes, vTileOuterCodes);

	// 去掉边界范围外的图幅列表
	vector<string> vTileResults;
	vTileResults.clear();
	_del_outer(vTileOuterCodes, lon_min, lat_min, lon_max, lat_max, vTileResults);

	for (int i = 0; i < vTileInnerCodes.size(); i++)
	{
		vTileCodes.push_back(vTileInnerCodes[i]);
	}

	for (int i = 0; i < vTileResults.size(); i++)
	{
		vTileCodes.push_back(vTileResults[i]);
	}
}

string CSE_MapSheet::gb_to_gjb(string gb_name, string map_type)
{
	// gb2012转jb2008图幅号
	// 如果地图类型不是"D"、"K"、"L"，返回空字符串
	if (map_type != "D"
		&& map_type != "K"
		&& map_type != "L")
	{
		return "";
	}

	if (map_type == "K")
	{
		// K图幅目前仅支持500k、250k，长度应为10或11
		if (gb_name.length() > 11 || gb_name.length() < 10)
		{
			return "";
		}

		// 判断图幅号第四位或第五位是否为B或C
		string strScaleCode;
		if (gb_name.length() == 10)
		{
			strScaleCode = gb_name.substr(3, 1);
		}
		else if (gb_name.length() == 11)
		{
			strScaleCode = gb_name.substr(4, 1);
		}

		
		if (strScaleCode != "B"
			&& strScaleCode != "C")
		{
			return "";
		}

	
	}

	else if (map_type == "L")
	{
		// 长度为10或11的L图幅，倒数第七位应为B或C
		if (gb_name.length() == 10 ||
			gb_name.length() == 11)
		{
			// 判断图幅号第四位是否为B或C
			string strScaleCode;
			if (gb_name.length() == 10)
			{
				strScaleCode = gb_name.substr(3, 1);
			}
			else if (gb_name.length() == 11)
			{
				strScaleCode = gb_name.substr(4, 1);
			}

			if (strScaleCode != "B"
				&& strScaleCode != "C")
			{
				return "";
			}
		}
	}


	if (gb_name.length() == 10 || gb_name.length() == 11)
	{
		string strScaleCode;
		if (gb_name.length() == 10)
		{
			strScaleCode = gb_name.substr(3, 1);
		}
		else if (gb_name.length() == 11)
		{
			strScaleCode = gb_name.substr(4, 1);
		}

		if (strScaleCode == "B"
			|| strScaleCode == "C"
			|| strScaleCode == "D")
		{
			return _gb2012_to_gjb2008_500k_250k_100k(gb_name, map_type);
		}
	}

	if (gb_name.length() == 10 || gb_name.length() == 11)
	{
		string strScaleCode;
		if (gb_name.length() == 10)
		{
			strScaleCode = gb_name.substr(3, 1);
		}
		else if (gb_name.length() == 11)
		{
			strScaleCode = gb_name.substr(4, 1);
		}

		if (strScaleCode == "E"
			|| strScaleCode == "F"
			|| strScaleCode == "G"
			|| strScaleCode == "H")
		{
			return _gb2012_to_gjb2008_50k_25k_10k_5k(gb_name, map_type);
		}
	}

	if (gb_name.length() == 3 || gb_name.length() == 4)
	{
		return _gb2012_to_gjb2008_1m(gb_name, map_type);
	}

	return "";
}


string CSE_MapSheet::get_name_from_latlon(double scale, double lon_input, double lat_input, string map_type)
{
	string strSheet;

	// 经度范围[-180,180]，纬度范围[-90,90]
	if (fabs(lon_input) > 180
		|| fabs(lat_input) > 88)
	{
		return "";
	}

	double lat_max = 0;
	double lat_min = 0;
	if (fabs(lat_input - (-88.0)) <= EQUAL_VALUE)
	{
		lat_max = -84.0;
		lat_min = -88.0;
	}
	else if (fabs(88.0 - lat_input) <= EQUAL_VALUE)
	{
		lat_max = 88.0;
		lat_min = 84.0;
	}
	else
	{
		if (lat_input < 0)
		{
			lat_max = -int(fabs(lat_input)) / 4 * 4;
		}
		else
		{
			lat_max = int(lat_input) / 4 * 4 + 4;
		}

		lat_min = lat_max - 4;
	}

	// 输入点所在区域图幅列表
	vector<string> vSheetList;
	vSheetList.clear();

	int lon_diff_1m_list[3] = { 6,12,24 };
	double lon_max = 0;
	while (lat_max > lat_min)
	{
		double lat = lat_max;
		// 计算纬度所在分区，分1、2、3三个分区，[-60.0, 60.0)、[60,76) or [-76,-60)、[76,88] or [-88,-76]
		int lat_region = get_lat_region(lat - 4);
		if (lat_region == 0)
		{
			return "";
		}
		// 经度差
		double lon_diff_1m = lon_diff_1m_list[lat_region - 1];
		double lon_start = 0;
		if ((lon_input - (-180)) <= EQUAL_VALUE)
		{
			lon_start = -180;
			lon_max = -180 + lon_diff_1m;
		}
		else if (fabs(lon_diff_1m - 24) <= EQUAL_VALUE)
		{
			if (int(lon_input + 12) % int(lon_diff_1m) <= EQUAL_VALUE)
			{ 
				lon_start = lon_input - lon_diff_1m;
				lon_max = lon_input + lon_diff_1m;
			}
			else
			{
				lon_start = (int(lon_input + 12) / int(lon_diff_1m)) * lon_diff_1m - 12;
				lon_max = lon_start + lon_diff_1m;
			}

		}
		else
		{
			if ((int(lon_input) % int(lon_diff_1m)) <= EQUAL_VALUE)
			{
				lon_start = lon_input - lon_diff_1m;
				lon_max = lon_input + lon_diff_1m;
			}
			else
			{
				lon_start = (int(lon_input) / int(lon_diff_1m)) * lon_diff_1m;
				lon_max = lon_start + lon_diff_1m;
			}
		}

		if (lon_max > 180)
		{
			lon_max = 180;
		}
		
		while (lon_start < lon_max)
		{
			vector<string> name_list;
			name_list.clear();
			double lon = lon_start;

			// 获取100万名字
			string name_1m = get_name_1m(lon * RAD8, lat * RAD8, map_type);  // 左上角
			double lon_min_1m;
			double lat_min_1m;
			double lon_max_1m;
			double lat_max_1m;
			get_box(name_1m, lon_min_1m, lat_max_1m, lon_max_1m, lat_min_1m);

			if (lon_min_1m > lon_input
				|| lat_min_1m > lat_input
				|| lon_max_1m < lon_input
				|| lat_max_1m < lat_input)
			{
				lon_start += lon_diff_1m;
				continue;
			}
			else
			{
				// 获取百万图幅对应某比例尺的所有图幅列表
				if (fabs(scale - SCALE_1M) < DOUBLE_EQUAL_ZERO)
				{
					name_list.push_back(name_1m);
				}
				else
				{
					name_list = get_large_scale_name_list(name_1m, SCALE_1M, scale);
				}
			}

			for (int i = 0; i < name_list.size(); i++)
			{
				vSheetList.push_back(name_list[i]);
			}
			
			lon_start += lon_diff_1m;
		}

		lat_max -= 4;
	}

	// 过滤出经纬度点所在的图幅名
	strSheet = _find_name_list(vSheetList, lon_input, lat_input);

	return strSheet;
}

void CSE_MapSheet::get_adjsheet_by_scale_and_sheet(double scale, 
	string strSheet, 
	string& strLeftSheet, 
	string& strTopSheet, 
	string& strRightSheet, 
	string& strBottomSheet, 
	string map_type)
{
	// 计算当前图幅经纬度范围
	double dLeft = 0;
	double dTop = 0;
	double dRight = 0;
	double dBottom = 0;

	get_box(strSheet, dLeft, dTop, dRight, dBottom);

	// 计算经度差、纬度差，构建上、下、左、右图幅的中心点坐标
	double dWidth = dRight - dLeft;
	double dHeight = dTop - dBottom;

	// 当前图幅中心点坐标
	double dCenterPointX = dLeft + dWidth * 0.5;
	double dCenterPointY = dBottom + dHeight * 0.5;

	// 上图幅中心点坐标
	double dTopCenterPointX = dCenterPointX;
	double dTopCenterPointY = dCenterPointY + dHeight;

	// 下图幅中心点坐标
	double dBottomCenterPointX = dCenterPointX;
	double dBottomCenterPointY = dCenterPointY - dHeight;

	// 左图幅中心点坐标
	double dLeftCenterPointX = dCenterPointX - dWidth;
	double dLeftCenterPointY = dCenterPointY;

	// 右图幅中心点坐标
	double dRightCenterPointX = dCenterPointX + dWidth;
	double dRightCenterPointY = dCenterPointY;

	// 分别计算上、下、左、右中心点所在图幅名
	strTopSheet = get_name_from_latlon(scale, dTopCenterPointX, dTopCenterPointY, map_type);
	strBottomSheet = get_name_from_latlon(scale, dBottomCenterPointX, dBottomCenterPointY, map_type);
	strLeftSheet = get_name_from_latlon(scale, dLeftCenterPointX, dLeftCenterPointY, map_type);
	strRightSheet = get_name_from_latlon(scale, dRightCenterPointX, dRightCenterPointY, map_type);
}

string CSE_MapSheet::_gb2012_to_gjb2008_500k_250k_100k(string gb_name,string map_type)
{
	// 50万、25万、10万比例尺，2012版国标图幅名，转2008版军标图幅名
	//  jb_name = 'DN10501'         jb_name = 'DS10501'
	//	gb_name = 'J50B001001'      gb_name = 'SJ50B001001'
	// 获取比例尺编码
	string strScaleCode;
	if (gb_name.length() == 10)
	{
		strScaleCode = gb_name.substr(3, 1);
	}
	else if (gb_name.length() == 11)
	{
		strScaleCode = gb_name.substr(4, 1);
	}

	int div_val = 0;
	int seq_len = 0;

	// 50万
	if (strScaleCode == "B")
	{
		div_val = 2;
		seq_len = 1;
	}
	// 25万
	else if (strScaleCode == "C")
	{
		div_val = 4;
		seq_len = 2;
	}
	// 10万
	else if (strScaleCode == "D")
	{
		div_val = 12;
		seq_len = 3;
	}

	else
	{
		return "";
	}
	
	// 行号
	string strRowNum = gb_name.substr(gb_name.length() - 6, 3);

	// 列号
	string strColNum = gb_name.substr(gb_name.length() - 3, 3);

	int gb_row_num = atoi(strRowNum.c_str());
	int gb_column_num = atoi(strColNum.c_str());

	// 100万图幅名称
	string gb_1m_name;
	// 北半球
	if (gb_name.length() == 10)
	{
		gb_1m_name = gb_name.substr(0, 3);
	}
	// 南半球
	else if (gb_name.length() == 11)
	{
		gb_1m_name = gb_name.substr(1, 3);
	}
	
	string jb_1m_name = _gb2012_to_gjb2008_1m(gb_1m_name, map_type);
	int jb_seq = (gb_row_num - 1) * div_val + gb_column_num;

	// 军标编码
	string jb_name;
	char sz_jb_seq[10] = { 0 };
	if (seq_len == 1)
	{
		sprintf(sz_jb_seq, "%d", jb_seq);
		string str_jb_seq = sz_jb_seq;
		jb_name = jb_1m_name + str_jb_seq;
	}
	else
	{
		sprintf(sz_jb_seq, "%d", jb_seq);
		string str_jb_seq = sz_jb_seq;

		size_t iZeroCount = seq_len - str_jb_seq.length();
		string strZero;
		for (int i = 0; i < iZeroCount; i++)
		{
			strZero += "0";
		}
		jb_name = jb_1m_name + strZero + str_jb_seq;
	}
	return jb_name;
}

string CSE_MapSheet::_gb2012_to_gjb2008_50k_25k_10k_5k(string gb_name, string map_type)
{
	/*5万、2.5万、1万比例尺，2012版国标图幅名，转2008版军标图幅名,
	（由于文档50k_25k_10k国转军公式不全，该函数用50k_25k_10k军转国公式逆推求解的）*/
	
	// 获取比例尺编码
	string strScaleCode;
	if (gb_name.length() == 10)
	{
		strScaleCode = gb_name.substr(3, 1);
	}
	else if (gb_name.length() == 11)
	{
		strScaleCode = gb_name.substr(4, 1);
	}

	// 国标上一级编码
	string father_gb_type;
	// 5万
	if (strScaleCode == "E")
	{
		father_gb_type = "D";
	}
	// 2.5万
	else if (strScaleCode == "F")
	{
		father_gb_type = "E";
	}
	// 1万
	else if (strScaleCode == "G")
	{
		father_gb_type = "F";
	}
	// 5千
	else if (strScaleCode == "H")
	{
		father_gb_type = "G";
	}
	else
	{
		return "";
	}

	// 行号
	string strRowNum = gb_name.substr(gb_name.length() - 6, 3);

	// 列号
	string strColNum = gb_name.substr(gb_name.length() - 3, 3);

	int gb_row_num = atoi(strRowNum.c_str());
	int gb_column_num = atoi(strColNum.c_str());

	// 国标100万图幅名称
	string gb_1m_name;
	// 北半球
	if (gb_name.length() == 10)
	{
		gb_1m_name = gb_name.substr(0, 3);
	}
	// 南半球
	else if (gb_name.length() == 11)
	{
		gb_1m_name = gb_name.substr(1, 3);
	}

	int theta = 0;
	int father_gb_row_num = 0;
	int father_gb_column_num = 0;

	if (gb_row_num % 2 == 0 && gb_column_num % 2 == 0)
	{
		theta = 4;
		father_gb_row_num = int(gb_row_num / 2);
		father_gb_column_num = int(gb_column_num / 2);
	}
	else if (gb_row_num % 2 == 0 && gb_column_num % 2 != 0)
	{
		theta = 3;
		father_gb_row_num = int(gb_row_num / 2);
		father_gb_column_num = int((gb_column_num + 1) / 2);
	}
	else if (gb_row_num % 2 != 0 && gb_column_num % 2 == 0)
	{
		theta = 2;
		father_gb_row_num = int((gb_row_num + 1) / 2);
		father_gb_column_num = int(gb_column_num / 2);
	}
	else
	{
		theta = 1;
		father_gb_row_num = int((gb_row_num + 1) / 2);
		father_gb_column_num = int((gb_column_num + 1) / 2);
	}
	
	// 行编码中"0"的个数
	string str_father_gb_row_num = to_string(father_gb_row_num);
	size_t iRowZeroCount = 3 - str_father_gb_row_num.length();
	for (int i = 0; i < iRowZeroCount; i++)
	{
		str_father_gb_row_num = "0" + str_father_gb_row_num;
	}

	// 列编码中"0"的个数
	string str_father_gb_column_num = to_string(father_gb_column_num);
	size_t iColZeroCount = 3 - str_father_gb_column_num.length();
	for (int i = 0; i < iColZeroCount; i++)
	{
		str_father_gb_column_num = "0" + str_father_gb_column_num;
	}

	string father_gb_name = gb_1m_name + father_gb_type + str_father_gb_row_num + str_father_gb_column_num;
	
	if (father_gb_type == "D")
	{
		father_gb_name = _gb2012_to_gjb2008_500k_250k_100k(father_gb_name, map_type);
	}
	else
	{
		father_gb_name = _gb2012_to_gjb2008_50k_25k_10k_5k(father_gb_name, map_type);
	}

	string jb_name = father_gb_name + to_string(theta);

	return jb_name;
}

string CSE_MapSheet::_gb2012_to_gjb2008_1m(string gb_name, string map_type)
{
	// 100万比例尺，2012版国标图幅名，转2008版军标图幅名
	// jb_name = 'DN1047'  jb_name = 'DS1047'
	// gb_name = 'G47'     gb_name = 'SG47'
	
	string str_gb_row;
	string gb_column_num;
	
		// 北半球
	if (gb_name.length() == 3)
	{
		str_gb_row = gb_name.substr(0, 1);
		gb_column_num = gb_name.substr(1, 2);
	}
	// 南半球
	else if (gb_name.length() == 4)
	{
		str_gb_row = gb_name.substr(1, 1);
		gb_column_num = gb_name.substr(2, 2);
	}

	vector<string> gb_1m_row_list;
	gb_1m_row_list.clear();

	gb_1m_row_list.push_back("A");
	gb_1m_row_list.push_back("B");
	gb_1m_row_list.push_back("C");
	gb_1m_row_list.push_back("D");
	gb_1m_row_list.push_back("E");

	gb_1m_row_list.push_back("F");
	gb_1m_row_list.push_back("G");
	gb_1m_row_list.push_back("H");
	gb_1m_row_list.push_back("I");
	gb_1m_row_list.push_back("J");

	gb_1m_row_list.push_back("K");
	gb_1m_row_list.push_back("L");
	gb_1m_row_list.push_back("M");
	gb_1m_row_list.push_back("N");
	gb_1m_row_list.push_back("O");

	gb_1m_row_list.push_back("P");
	gb_1m_row_list.push_back("Q");
	gb_1m_row_list.push_back("R");
	gb_1m_row_list.push_back("S");
	gb_1m_row_list.push_back("T");

	gb_1m_row_list.push_back("U");
	gb_1m_row_list.push_back("V");

	// 行号索引
	int jb_row_num = 0;
	for (int i = 0; i < gb_1m_row_list.size(); i++)
	{
		if (str_gb_row == gb_1m_row_list[i])
		{
			jb_row_num = i + 1;
			break;
		}
	}
	
	char sz_jb_row_num[5] = { 0 };
	sprintf(sz_jb_row_num, "%d", jb_row_num);
	string str_jb_row_num;

	// 军标图幅号
	string jb_name;
	if (jb_row_num < 10)
	{
		string strTemp = sz_jb_row_num;
		str_jb_row_num = "0" + strTemp;
	}
	else
	{
		str_jb_row_num = sz_jb_row_num;
	}

	jb_name = str_jb_row_num + gb_column_num;

	string strFlag = gb_name.substr(0, 1);
	// 南半球
	if (gb_name.length() > 3
		&& strFlag == "S")
	{
		jb_name = map_type + "S" + jb_name;
	}
	// 北半球
	else
	{
		jb_name = map_type + "N" + jb_name;
	}

	return jb_name;
}

/*	根据经纬度范围[lon_min, lat_min, lon_max, lat_max]，获取百万图幅下的所有某比例尺图幅列表,
	并返回范围内及其边界处图幅列表，默认范围[-180, -88, 180, 88]*/
void CSE_MapSheet::_get_name_list(double scale, string map_type, double lon_min, double lat_min, double lon_max, double lat_max, vector<string> &vTileInnerCodes, vector<string>& vTileOuterCodes)
{
	vector<string> vTileCodes;
	vTileCodes.clear();

	if (lon_min < -180 
		|| lon_max > 180
		|| lat_min < -88
		|| lat_max > 88)
	{
		return;
	}

	// 如果纬度÷4不能整除
	if (int(lat_max) % 4 > EQUAL_VALUE)
	{
		lat_max = (int(lat_max) / 4 + 1) * 4;
	}

	int lon_diff_1m_list[3] = { 6,12,24 };
	while (lat_max > lat_min)
	{
		double lat = lat_max;
		// 计算纬度所在分区，分1、2、3三个分区，[-60.0, 60.0)、[60,76) or [-76,-60)、[76,88] or [-88,-76]
		int lat_region = get_lat_region(lat - 4);
		if (lat_region == 0)
		{
			return;
		}
		// 经度差
		double lon_diff_1m = lon_diff_1m_list[lat_region - 1];
		double lon_start = 0;
		if ((lon_min - (-180)) <= EQUAL_VALUE)
		{
			lon_start = -180;
		}
		else if (fabs(lon_diff_1m - 24) <= EQUAL_VALUE)
		{
			if (int(lon_min + 12) % int(lon_diff_1m) <= EQUAL_VALUE)
				lon_start = lon_min - lon_diff_1m;
			else
				lon_start = (int(lon_min + 12) / int(lon_diff_1m)) * lon_diff_1m - 12;
		}
		else
		{
			if ((int(lon_min) % int(lon_diff_1m)) <= EQUAL_VALUE)
				lon_start = lon_min - lon_diff_1m;
			else
				lon_start = (int(lon_min) / int(lon_diff_1m)) * lon_diff_1m;
		}

		while (lon_start < lon_max)
		{
			vector<string> name_list;
			name_list.clear();
			double lon = lon_start;

			// 获取100万名字
			string name_1m = get_name_1m(lon * RAD8, lat * RAD8, map_type);  // 左上角

			// 获取百万图幅对应某比例尺的所有图幅列表
			if (fabs(scale - SCALE_1M) < DOUBLE_EQUAL_ZERO)
			{
				name_list.push_back(name_1m);
			}
			else
			{
				name_list = get_large_scale_name_list(name_1m, SCALE_1M, scale);
			}			

			// 范围内和边界处图幅分开存到序列中
			if (fabs(lat - lat_max) < EQUAL_VALUE
				|| (lat - 4) < lat_min)
			{
				for (int i = 0; i < name_list.size(); i++ )
				{
					vTileOuterCodes.push_back(name_list[i]);
				}
			}
			else
			{
				if (lon <= lon_min || (lon + lon_diff_1m) > lon_max)
				{
					for (int i = 0; i < name_list.size(); i++)
					{
						vTileOuterCodes.push_back(name_list[i]);
					}
				}
				else
				{
					for (int i = 0; i < name_list.size(); i++)
					{
						vTileInnerCodes.push_back(name_list[i]);
					}
				}
			}
			lon_start += lon_diff_1m;
		}

		lat_max -= 4;
	}


}

/*删除给定范围边界外的图幅号, 并返回边界处属于范围内的图幅列表
	: param bbox : 给定经纬度范围[lon_min, lat_min, lon_max, lat_max], 默认[-180, -88, 180, 88]
	: param name_outer : 给定范围的图幅列表
	:return : 边界处属于范围内的图幅列表*/
void CSE_MapSheet::_del_outer(vector<string> vTileCodes,double lon_min,double lat_min,double lon_max,double lat_max, vector<string> &vTileCodesResult)
{
	vTileCodesResult.clear();
	int i = 0;
	double lon_min_50k = 0;
	double lat_min_50k = 0;
	double lon_max_50k = 0;
	double lat_max_50k = 0;
	for (i = 0; i < vTileCodes.size(); i++)
	{
		// 将不在图幅范围内的图幅号去掉
		get_box(vTileCodes[i], lon_min_50k, lat_max_50k, lon_max_50k, lat_min_50k);
		if (lon_min_50k >= lon_max
			|| lat_min_50k >= lat_max
			|| lon_max_50k <= lon_min
			|| lat_max_50k <= lat_min)
		{
			continue;
		}
		vTileCodesResult.push_back(vTileCodes[i]);
	}
}



double CSE_MapSheet::get_scale(string strCode)
{
	// 查找图幅号中的N或S，如果是DN或DS开头的图幅号，则去掉D
	size_t iIndex = strCode.find('N');
	string strCalCode;		// 待计算图幅号的编码
	if (iIndex != -1)		// 如果找到N，则说明是北半球编码
	{
		if (iIndex != 0)	// 如果首位不是0，则需要把N前面的D去掉
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}
	else    // 说明是南半球
	{
		iIndex = strCode.find('S');
		if (iIndex != 0)
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}

	// 如果是100万比例尺（N1050）
	if (strCalCode.length() == 5)
	{
		return 1000000;
	}
	// 如果是50万比例尺
	else if (strCalCode.length() == 6)
	{
		return 500000;
	}
	// 如果是25万比例尺
	else if (strCalCode.length() == 7)
	{
		return 250000;
	}
	// 如果是10万比例尺
	else if (strCalCode.length() == 8)
	{
		return 100000;
	}
	// 如果是5万比例尺
	else if (strCalCode.length() == 9)
	{
		return 50000;
	}
	// 如果是2.5万比例尺
	else if (strCalCode.length() == 10)
	{
		return 25000;
	}
	// 如果是1万比例尺
	else if (strCalCode.length() == 11)
	{
		return 10000;
	}
	// 如果是5千比例尺
	else if (strCalCode.length() == 12)
	{
		return 5000;
	}
	else if (strCalCode.length() > 12)
	{
		return 0;
	}
	return 0;
}

/*
根据2008版jb图幅名计算经纬度范围[lon_min, lat_min, lon_max, lat_max]

仅支持地形图D 10k - 5k(新增1:5000, 11位)、航空图K 250k - 500k(1m和2m暂时不支持)、联合作战图L 250k - 1m
暂时不支持航空图1m和2m
不支持航海图和专题图

图幅名全是数字时，默认北半球地形图
	: param name : 图幅名
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]
*/

bool CSE_MapSheet::get_box(string strCode, double& left, double& top, double& right, double& bottom)
{
	//  [6/5/2022 pippo]
	// 增加400万比例尺图幅计算
	/*以100万分幅数据为基础，以西经180度起算，赤道以北和以南，由N/S标识，进行400W分幅划分，将全球划分为180个400W图幅，在高纬度地区80°~88°，400W图幅只包含8个百万图幅范围。
		图幅范围：每16幅百万区域为一幅400万图幅，
		命名方式：命名长度为10个字符
		前两位与百万图幅一致，L表示LHZZT，N、S表示北纬和南纬；
		第3位到第6位为纬度方向最小、最大编码组合；
		第7位到第10位为经度方向最小、最大编码组合；
	*/
	// 如果是400w比例尺
	if ((strCode.find("LN") != -1 || strCode.find("LS") != -1)
		&& strCode.length() == 10)
	{
		// 纬度最小编码，3到4位
		int iLatFromCode = atoi(strCode.substr(2, 2).c_str());

		// 纬度最大编码，5到6位
		int iLatToCode = atoi(strCode.substr(4, 2).c_str());

		// 经度最小编码，7到8位
		int iLonFromCode = atoi(strCode.substr(6, 2).c_str());

		// 经度最大编码，9到10位
		int iLonToCode = atoi(strCode.substr(8, 2).c_str());

		size_t iIndexOfN = strCode.find('N');
		// 如果找到N，则说明是北半球编码
		if (iIndexOfN != -1)
		{
			left = -180 + (iLonFromCode - 1) * 6;
			right = left + 24;
			bottom = (iLatFromCode - 1) * 4;
			top = (iLatToCode) * 4;
		}
		else
		{
			left = -180 + (iLonFromCode - 1) * 6;
			right = left + 24;
			top = -(iLatFromCode - 1) * 4;
			bottom = -(iLatToCode) * 4;
		}
		return true;
	}

	// 查找图幅号中的N或S，如果是DN或DS开头的图幅号，则去掉D
	size_t iIndex = strCode.find('N');
	string strCalCode;		// 待计算图幅号的编码
	if (iIndex != -1)		// 如果找到N，则说明是北半球编码
	{
		if (iIndex != 0)	// 如果首位不是0，则需要把N前面的D去掉
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}
	else    // 说明是南半球
	{
		iIndex = strCode.find('S');
		if (iIndex != 0)
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}

	// 如果是100万比例尺（N1050）
	if (strCalCode.length() == 5)
	{
		return get_box_1m(strCalCode, left, top, right, bottom);
	}
	// 如果是50万比例尺
	else if (strCalCode.length() == 6)
	{
		return get_box_500k_250k_100k_2008(strCalCode, 500000, left, top, right, bottom);
	}
	// 如果是25万比例尺
	else if (strCalCode.length() == 7)
	{
		return get_box_500k_250k_100k_2008(strCalCode, 250000, left, top, right, bottom);
	}
	// 如果是10万比例尺
	else if (strCalCode.length() == 8)
	{
		return get_box_500k_250k_100k_2008(strCalCode, 100000, left, top, right, bottom);
	}
	// 如果是5万比例尺
	else if (strCalCode.length() == 9)
	{
		return get_bbox_50k_2008(strCalCode, 50000, left, top, right, bottom);
	}
	// 如果是2.5万比例尺
	else if (strCalCode.length() == 10)
	{
		return get_bbox_25k_2008(strCalCode, 25000, left, top, right, bottom);
	}
	// 如果是1万比例尺
	else if (strCalCode.length() == 11)
	{
		return get_bbox_10k_2008(strCalCode, 10000, left, top, right, bottom);
	}
	// 如果是5千比例尺
	else if (strCalCode.length() == 12)
	{
		return get_bbox_5k_2008(strCalCode, 5000, left, top, right, bottom);
	}
	else if (strCalCode.length() > 12)
	{
		return false;
	}
	return true;
}

bool CSE_MapSheet::get_box_1m(string strCode, double& left, double& top, double& right, double& bottom)
{
	_lat_1m(strCode, top, bottom);
	_lon_1m(strCode, right, left);
	return true;
}

// 获取百万图幅的纬度边界（N1050）
bool CSE_MapSheet::_lat_1m(string strCode, double& dLatMax, double& dLatMin)
{
	string strRow = strCode.substr(1, 2);
	int iRow = atoi(strRow.c_str());

	double latMax = 4 * iRow;
	double latMin = 4 * (iRow - 1);

	// 南半球
	if (strCode.length() > 4 && strCode.find('S') != -1)
	{
		dLatMax = -latMin;
		dLatMin = -latMax;
	}
	// 北半球
	else
	{
		dLatMax = latMax;
		dLatMin = latMin;
	}
	return true;
}


// 获取百万图幅的纬度边界（N1050）
bool CSE_MapSheet::_lon_1m(string strCode, double& dLonMax, double& dLonMin)
{
	string strRow = strCode.substr(1, 2);
	int iRow = atoi(strRow.c_str());

	string strCol = strCode.substr(3, 2);
	int iCol = atoi(strCol.c_str());

	if (iRow <= 22)
	{
		if (iCol > 60)
		{
			return false;
		}
		if (iCol > 30)
		{
			dLonMax = 6 * (iCol - 30);
			dLonMin = 6 * (iCol - 31);
		}
		else
		{
			dLonMax = -6 * (30 - iCol);
			dLonMin = -6 * (31 - iCol);
		}
	}
	else  // 图幅名不合法:前两位数字应<=22
	{
		return false;
	}

	return true;
}

// 获取50万、25万、10万图幅范围
bool CSE_MapSheet::get_box_500k_250k_100k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*
	def _bbox_500k_250k_100k_2008(name, scale) :
	"""
	根据50万、25万、10万图幅名、纬度差、经度差、总行数或总列数，求2008版50万、25万、10万经纬度范围
	: param name : 图幅名
	:param lat_diff : 图幅纬度差
	:param lon_diff : 图幅经度差
	:param row_sum : 图幅从父级能分割成的总行数或总列数
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]
	"""*/

	// 100万图幅编码
	string str1mCode = strCode.substr(0, 5);

	// 获取100万图幅的经纬度范围
	double dLatMin = 0;
	double dLatMax = 0;
	double dLonMin = 0;
	double dLonMax = 0;
	bool bResult = false;
	bResult = get_box_1m(str1mCode, dLonMin, dLatMax, dLonMax, dLatMin);
	if (!bResult)
	{
		return false;
	}

	int sequence_num = atoi(strCode.substr(5, strCode.length()).c_str());        //int(name[5:])   # 图幅名去除百万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(dLonMin, dLatMin, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

int CSE_MapSheet::_get_row_nums(double scale)
{
	/* 	根据比例尺计算分幅行数，
	行数与列数相同
		例如，100万分为50万：2行2列
		: param scale : 比例尺
		:return : 分幅行数*/
		/*# 地形图各比例尺从父级得到的分幅行数, 10万到500万的图幅
			ROW_COLUMN_NUM_DIC = { SCALE_500K: 2,
			SCALE_250K : 4,
			SCALE_100K : 12,
			SCALE_50K : 2,
			SCALE_25K : 2,
			SCALE_10K : 2,
			SCALE_5K : 2
		}*/

		// 50万比例尺
	if (fabs(scale - 500000) < 1e-6)
	{
		return 2;
	}
	// 25万比例尺
	else if (fabs(scale - 250000) < 1e-6)
	{
		return 4;
	}
	// 10万比例尺
	else if (fabs(scale - 100000) < 1e-6)
	{
		return 12;
	}
	// 5万比例尺
	else if (fabs(scale - 50000) < 1e-6)
	{
		return 2;
	}
	// 2.5万比例尺
	else if (fabs(scale - 25000) < 1e-6)
	{
		return 2;
	}
	// 1万比例尺
	else if (fabs(scale - 10000) < 1e-6)
	{
		return 2;
	}
	// 5k比例尺
	else if (fabs(scale - 5000) < 1e-6)
	{
		return 2;
	}
	else
	{
		return 2;
	}
}

bool CSE_MapSheet::_calculate_row_column(int sequence_num, int row_sum, int& row, int& column)
{
	/*根据序号和总行数（或总列数），计算所属行列号
		: param sequence_num : 序号
		:param row_sum : 总行数（或总列数）
		:return : 行号和列号 row, column*/

	if (sequence_num > row_sum)
	{
		// 商:row，余数:colume
		row = int(sequence_num / row_sum);
		column = sequence_num % row_sum;

		if (column == 0)
		{
			row = row;
			column = row_sum;
		}
		else
		{
			row += 1;
			column = column;
		}
	}

	else
	{
		row = 1;
		column = sequence_num;
	}
	return true;
}

void CSE_MapSheet::_calculate_bbox(double lon_left,
	double lat_bottom,
	double scale,
	int row,
	int column,
	int row_sum,
	double& lon_min,
	double& lat_min,
	double& lon_max,
	double& lat_max)
{
	/*
	求图幅经纬度范围
	: param lon_left : 图幅所属100万左下角经度
	:param lat_bottom : 图幅所属100万左下角纬度
	:param scale : 当前图幅比例尺
	:param row : 行号
	:param column : 列号
	:param row_sum : 总行数或总列数
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/

	double lat_diff = get_lat_diff(scale);
	double lon_diff = get_lon_diff(scale);

	double rad8 = 8 * 60 * 60;
	lat_min = (lat_bottom * rad8 + (row_sum - row) * lat_diff) / rad8;
	lat_max = (lat_bottom * rad8 + (row_sum - row + 1) * lat_diff) / rad8;
	lon_min = (lon_left * rad8 + (column - 1) * lon_diff) / rad8;
	lon_max = (lon_left * rad8 + column * lon_diff) / rad8;
}

double CSE_MapSheet::get_lat_diff(double scale)
{
	/*
	根据比例尺计算纬度差

	: param scale : 比例尺
	:return : 纬度差*/

	//return lat_diff_dic[scale]
	/*# 地形图各比例尺纬度差，每秒乘8。
		LAT_DIFF_RAD_DIC = { SCALE_1M: 115200,
		SCALE_500K : 57600,
		SCALE_250K : 28800,
		SCALE_100K : 9600,
		SCALE_50K : 4800,
		SCALE_25K : 2400,
		SCALE_10K : 1200,
		SCALE_5K : 600,
		SCALE_2K : 200,
		SCALE_1K : 100,
		SCALE_500 : 50
	}*/
	double lat_diff = 0;
	if (fabs(scale - 1000000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 115200;
	}
	else if (fabs(scale - 500000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 57600;
	}
	if (fabs(scale - 250000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 28800;
	}
	else if (fabs(scale - 100000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 9600;
	}
	else if (fabs(scale - 50000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 4800;
	}
	else if (fabs(scale - 25000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 2400;
	}
	else if (fabs(scale - 10000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 1200;
	}
	else if (fabs(scale - 5000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 600;
	}
	else if (fabs(scale - 2000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 200;
	}
	else if (fabs(scale - 1000) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 100;
	}
	else if (fabs(scale - 500) < DOUBLE_EQUAL_ZERO)
	{
		lat_diff = 50;
	}
	return lat_diff;
}


double CSE_MapSheet::get_lon_diff(double scale)
{
	/*根据比例尺计算各分区的经度差

	: param scale : 比例尺
	:return : 经度差*/
	/*# 地形图各比例尺不同纬度区域经度差，每秒乘8
		LON_DIFF_RAD_DIC = { SCALE_1M: [172800, 172800 * 2, 172800 * 4],
		SCALE_500K : [86400, 86400 * 2, 86400 * 4],
		SCALE_250K : [43200, 43200 * 2, 43200 * 4],
		SCALE_100K : [14400, 14400 * 2, 14400 * 4],
		SCALE_50K : [7200, 7200 * 2, 7200 * 4],
		SCALE_25K : [3600, 3600 * 2, 3600 * 4],
		SCALE_10K : [1800, 1800 * 2, 1800 * 4],
		SCALE_5K : [900, 900 * 2, 900 * 4],
		SCALE_2K : [300, 300 * 2, 300 * 4],
		SCALE_1K : [150, 150 * 2, 150 * 4],
		SCALE_500 : [75, 75 * 2, 75 * 4]
	}*/

	double lon_diff = 0;
	if (fabs(scale - 1000000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 172800;
	}
	else if (fabs(scale - 500000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 86400;
	}
	if (fabs(scale - 250000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 43200;
	}
	else if (fabs(scale - 100000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 14400;
	}
	else if (fabs(scale - 50000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 7200;
	}
	else if (fabs(scale - 25000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 3600;
	}
	else if (fabs(scale - 10000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 1800;
	}
	else if (fabs(scale - 5000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 900;
	}
	else if (fabs(scale - 2000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 300;
	}
	else if (fabs(scale - 1000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 150;
	}
	else if (fabs(scale - 500) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 75;
	}
	return lon_diff;
}

double CSE_MapSheet::get_lon_diff(double scale,int lat_region)
{
	/*根据比例尺计算各分区的经度差

	: param scale : 比例尺
	:return : 经度差*/
	/*# 地形图各比例尺不同纬度区域经度差，每秒乘8
		LON_DIFF_RAD_DIC = { SCALE_1M: [172800, 172800 * 2, 172800 * 4],
		SCALE_500K : [86400, 86400 * 2, 86400 * 4],
		SCALE_250K : [43200, 43200 * 2, 43200 * 4],
		SCALE_100K : [14400, 14400 * 2, 14400 * 4],
		SCALE_50K : [7200, 7200 * 2, 7200 * 4],
		SCALE_25K : [3600, 3600 * 2, 3600 * 4],
		SCALE_10K : [1800, 1800 * 2, 1800 * 4],
		SCALE_5K : [900, 900 * 2, 900 * 4],
		SCALE_2K : [300, 300 * 2, 300 * 4],
		SCALE_1K : [150, 150 * 2, 150 * 4],
		SCALE_500 : [75, 75 * 2, 75 * 4]
	}*/

	double lon_diff = 0;
	if (fabs(scale - 1000000) < DOUBLE_EQUAL_ZERO)
	{
		if (lat_region == 1)
		{
			lon_diff = 172800;
		}
		else if(lat_region == 2)
		{
			lon_diff = 172800 * 2;
		}
		else if(lat_region == 3)
		{
			lon_diff = 172800 * 4;
		}
		else
		{
			lon_diff = 172800;
		}
		
	}
	else if (fabs(scale - 500000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 86400;
	}
	if (fabs(scale - 250000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 43200;
	}
	else if (fabs(scale - 100000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 14400;
	}
	else if (fabs(scale - 50000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 7200;
	}
	else if (fabs(scale - 25000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 3600;
	}
	else if (fabs(scale - 10000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 1800;
	}
	else if (fabs(scale - 5000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 900;
	}
	else if (fabs(scale - 2000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 300;
	}
	else if (fabs(scale - 1000) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 150;
	}
	else if (fabs(scale - 500) < DOUBLE_EQUAL_ZERO)
	{
		lon_diff = 75;
	}
	return lon_diff;
}

// 获取5万图幅范围
bool CSE_MapSheet::get_bbox_50k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*根据图幅名，求2008版5万图幅范围
	: param name : 图幅名
	:param scale : 比例尺
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/
	string name_100k = strCode.substr(0, strCode.length() - 1);		// 取5万图幅编码的前n-1位

	double d100k_left = 0;
	double d100k_top = 0;
	double d100k_right = 0;
	double d100k_bottom = 0;

	// 获取10w图幅的范围
	get_box_500k_250k_100k_2008(name_100k, 100000, d100k_left, d100k_top, d100k_right, d100k_bottom);
	int sequence_num = atoi(strCode.substr(8, 1).c_str());	// 图幅名去除10万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d100k_left, d100k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}


// 获取2.5万图幅范围
bool CSE_MapSheet::get_bbox_25k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*根据图幅名，求2008版2.5万图幅范围
	: param name : 图幅名
	:param scale : 比例尺
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/
	string name_50k = strCode.substr(0, strCode.length() - 1);		// 取2.5万图幅编码的前n-1位

	double d50k_left = 0;
	double d50k_top = 0;
	double d50k_right = 0;
	double d50k_bottom = 0;

	// 获取5w图幅的范围
	get_bbox_50k_2008(name_50k, 50000, d50k_left, d50k_top, d50k_right, d50k_bottom);
	int sequence_num = atoi(strCode.substr(9, 1).c_str());	// 图幅名去除5万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d50k_left, d50k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

// 获取1万图幅范围
bool CSE_MapSheet::get_bbox_10k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*根据图幅名，求2008版1万图幅范围
	: param name : 图幅名
	:param scale : 比例尺
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/
	string name_25k = strCode.substr(0, strCode.length() - 1);		// 取1万图幅编码的前n-1位

	double d25k_left = 0;
	double d25k_top = 0;
	double d25k_right = 0;
	double d25k_bottom = 0;

	// 获取2.5w图幅的范围
	get_bbox_25k_2008(name_25k, 25000, d25k_left, d25k_top, d25k_right, d25k_bottom);
	int sequence_num = atoi(strCode.substr(10, 1).c_str());	// 图幅名去除2.5万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d25k_left, d25k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

// 获取5000图幅范围
bool CSE_MapSheet::get_bbox_5k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*根据图幅名，求2008版5k图幅范围
	: param name : 图幅名
	:param scale : 比例尺
	:return : 图幅经纬度范围[lon_min, lat_min, lon_max, lat_max]*/
	string name_10k = strCode.substr(0, strCode.length() - 1);		// 取5k图幅编码的前n-1位

	double d10k_left = 0;
	double d10k_top = 0;
	double d10k_right = 0;
	double d10k_bottom = 0;

	// 获取1w图幅的范围
	get_bbox_10k_2008(name_10k, 10000, d10k_left, d10k_top, d10k_right, d10k_bottom);
	int sequence_num = atoi(strCode.substr(11, 1).c_str());	// 图幅名去除1万图幅后的序列号
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d10k_left, d10k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

/*根据纬度计算纬度分区
	: param lat : 纬度
	:return : 纬度分区*/
int CSE_MapSheet::get_lat_region(double lat)
{
	// [-60.0, 60.0)
	if ((lat > -60.0 && lat < 60.0)
		|| fabs(lat + 60) <= EQUAL_VALUE)
	{
		return 1;
	}
	// [60,76) or [-76,-60)
	else if ((lat > 60.0 && lat < 76.0)
		|| (lat > -76.0 && lat < -60.0)
		|| fabs(lat - 60) <= EQUAL_VALUE
		|| fabs(lat - (-76)) <= EQUAL_VALUE)
	{
		return 2;
	}
	else if ((lat > 76.0 && lat < 88.0)
		|| (lat > -88.0 && lat < -76.0)
		|| fabs(lat - 76) <= EQUAL_VALUE
		|| fabs(lat - 88) <= EQUAL_VALUE
		|| fabs(lat + 76) <= EQUAL_VALUE
		|| fabs(lat + 88) <= EQUAL_VALUE)
	{
		return 3;
	}
	else
	{
		return 0;
	}
}

/*获取100万比例尺图幅名
	: param map_type : 地图类型，"D"表示地形图
	: param lon_start : 左上角经度, 等于 lon * 60 * 60 * 8
	: param lat_start : 左上角纬度, 等于 lat * 60 * 60 * 8

	: return : 100万比例尺图幅名*/
string CSE_MapSheet::get_name_1m( double lon_start, double lat_start,string map_type)
{
	double lat_diff = get_lat_diff(SCALE_1M);			// 纬度差 * 60 * 60 * 8
	double lat_region = get_lat_region(double(lat_start - lat_diff) / RAD8);  // 纬度分区
	string is_ns_1m = "";
	if ((lat_start - lat_diff) < 0)
		is_ns_1m = "S";
	else
	{
		is_ns_1m = "N";
	}
		
	double lon_diff = get_lon_diff(SCALE_1M, lat_region);

	string row_1m = _get_row_1m(lat_start, lat_diff);
	string column_1m = _get_column_1m(lat_region, lon_start, lon_diff);
	string name_1m = map_type + is_ns_1m + row_1m + column_1m;
	return name_1m;
}

/*计算100万比例尺行号，度转秒乘8再计算
	lat, 和lon都是左上角，

	计算要用左下角*/
string CSE_MapSheet::_get_row_1m(double lat_start, double lat_diff)
{
	string strRow_1m;
	char szRow_1m[10] = {0};
	int row_1m = 0;
	if ((lat_start - lat_diff) < 0)
		row_1m = int(round(abs(lat_start - lat_diff) / int(lat_diff)));
	else
		row_1m = int(round(lat_start / int(lat_diff)));
	if (row_1m < 10)
	{
		sprintf(szRow_1m, "0%d", row_1m);
	}
	else
	{
		sprintf(szRow_1m, "%d", row_1m);
	}
	strRow_1m = szRow_1m;
	return strRow_1m;
}

/*计算100万比例尺列号，度转秒乘8再计算*/
string CSE_MapSheet::_get_column_1m(double lat_region, double lon_start, double lon_diff)
{
	double x = 0;
	if (lat_region == 1)
		x = 31;
	else if (lat_region == 2)
		x = 16;
	else if (lat_region == 3)
		x = 8.5;
	else
		return "";
	string strCol_1m;
	char szCol_1m[10] = { 0 };
	int column_1m = int(round(double(lon_start) / lon_diff + x));
	if (column_1m < 10)
	{
		sprintf(szCol_1m, "0%d", column_1m);
	}
	else
	{
		sprintf(szCol_1m, "%d", column_1m);
	}
	strCol_1m = szCol_1m;
	return strCol_1m;
}

/*	根据小比例尺图幅名计算大比例尺图幅名列表

	:param name:当前图幅名
	:param scale:当前图幅的比例尺，范围：1万到100万(新增5k图幅范围，规范上没有该比例尺。)
	:param large_scale:大比例尺分母
	:return:大比例尺图幅名列表*/
vector<string> CSE_MapSheet::get_large_scale_name_list(string name, double scale, double large_scale)
{
	vector<string> vTileCodes;
	vTileCodes.clear();

	if (fabs(scale - SCALE_5K) < EQUAL_VALUE)
	{
		return vTileCodes;
	}

	if (scale <= large_scale)
	{
		return vTileCodes;
	}

	// 判断首字母
	string strTileType = name.substr(0, 1);
	if (strTileType == "K" && (large_scale != SCALE_500K && large_scale != SCALE_250K))
	{
		return vTileCodes;
	}

	if (strTileType == "L" && (large_scale != SCALE_1M && large_scale != SCALE_500K && large_scale != SCALE_250K))
	{
		return vTileCodes;
	}

	if (fabs(scale - SCALE_1M) < EQUAL_VALUE)
	{
		return _get_name_list_from_1m(name, large_scale);
	}
	else if (fabs(scale - SCALE_500K) < EQUAL_VALUE)
	{
		return _get_name_list_from_500k(name, large_scale);
	}
	else if (fabs(scale - SCALE_250K) < EQUAL_VALUE)
	{
		return _get_name_list_from_250k(name, large_scale);
	}
	else if (fabs(scale - SCALE_100K) < EQUAL_VALUE)
	{
		return _get_name_list_from_100k(name, large_scale);
	}
	else if (fabs(scale - SCALE_50K) < EQUAL_VALUE)
	{
		return _get_name_list_from_50k(name, large_scale);
	}
	else if (fabs(scale - SCALE_25K) < EQUAL_VALUE)
	{
		return _get_name_list_from_25k(name, large_scale);
	}
	else if (fabs(scale - SCALE_10K) < EQUAL_VALUE)
	{
		return _get_name_list_from_10k(name, large_scale);
	}
	else
	{
		return vTileCodes;
	}
					
}


vector<string> CSE_MapSheet::_get_name_list_from_1m(string name_1m, double child_scale)
{
	vector<string> vname;
	vname.clear();
	//获取百万图幅对应某比例尺的所有图幅列表
	if (fabs(child_scale - SCALE_500K) < EQUAL_VALUE)
	{
		return _get_name_500k_from_1m(name_1m);
	}
	else if (fabs(child_scale - SCALE_250K) < EQUAL_VALUE)
	{
		return _get_name_250k_from_1m(name_1m);
	}
	else if (fabs(child_scale - SCALE_100K) < EQUAL_VALUE)
	{
		return _get_name_100k_from_1m(name_1m);
	}
	else if (fabs(child_scale - SCALE_50K) < EQUAL_VALUE)
	{
		return _get_name_50k_from_1m(name_1m);
	}
	else if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)
	{
		return _get_name_25k_from_1m(name_1m);
	}
	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)
	{
		return _get_name_10k_from_1m(name_1m);
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)
	{
		return _get_name_5k_from_1m(name_1m);
	}
	else
	{
		return vname;
	}
	return vname;
}


	
/*根据百万图幅名, 获取对应的50万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_500k_from_1m(string name_1m)
{
	return _get_namelist_from_small_scale(name_1m);
}

/*根据上一级图幅名, 分成4份, 获取下一级图幅列表, 并返回

	100万-- > 50万
	10万-- > 5万
	5万-- > 2.5万
	2.5万-- > 1万
	1万-- > 5k*/
vector<string> CSE_MapSheet::_get_namelist_from_small_scale(string name_upper)
{
	vector<string> name_list;
	name_list.clear();
	for (int i = 1; i < 5; i++)
	{
		char szSeq[5] = { 0 };
		sprintf(szSeq, "%d", i);
		string strSeq = szSeq;
		string strName = name_upper + strSeq;
		name_list.push_back(strName);
	}
	return name_list;
}

/*100万图幅名, 获取25万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_250k_from_1m(string name_1m)
{
	vector<string> name_list;
	name_list.clear();
	for (int seq_250 = 1; seq_250 < 17; seq_250++)
	{
		char szSeq250[5] = { 0 };
		if (seq_250 < 10)
		{
			sprintf(szSeq250, "0%d", seq_250);
		}
		else
		{
			sprintf(szSeq250, "%d", seq_250);
		}

		string strSeq = szSeq250;
		string strName = name_1m + strSeq;
		name_list.push_back(strName);
	}
	return name_list;
}


/*100万图幅名, 获取10万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_100k_from_1m(string name_1m)
{
	vector<string> name_list;
	name_list.clear();
	for (int seq_100 = 1; seq_100 < 145; seq_100++)
	{
		char szSeq100[5] = { 0 };
		if (seq_100 < 10)
		{
			sprintf(szSeq100, "00%d", seq_100);
		}
		else if(seq_100 < 100)
		{
			sprintf(szSeq100, "0%d", seq_100);
		}
		else
		{
			sprintf(szSeq100, "%d", seq_100);
		}

		string strSeq = szSeq100;
		string strName = name_1m + strSeq;
		name_list.push_back(strName);
	}
	return name_list;
}

/*100万图幅名, 获取5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_50k_from_1m(string name_1m)
{
	vector<string> name_50k_list;
	name_50k_list.clear();
	vector<string> name_100k_list;
	name_100k_list.clear();
	name_100k_list = _get_name_100k_from_1m(name_1m);

	for (int i = 0; i < name_100k_list.size(); i++)
	{
		vector<string> vNameTemp;
		vNameTemp.clear();
		vNameTemp = _get_name_50k_from_100k(name_100k_list[i]);

		// 将vNameTemp中每个元素存储到name_50k_list中
		for (int j = 0; j < vNameTemp.size(); j++)
		{
			name_50k_list.push_back(vNameTemp[j]);
		}

	}

	return name_50k_list;
}

/*根据10万图幅名, 获取对应的5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_50k_from_100k(string name_100k)
{
	return _get_namelist_from_small_scale(name_100k);
}


/*100万图幅名, 获取2.5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_25k_from_1m(string name_1m)
{
	// 返回结果
	vector<string> name_25k_list;
	name_25k_list.clear();

	vector<string> name_50_list;
	name_50_list.clear();

	name_50_list = _get_name_50k_from_1m(name_1m);
	for (int i = 0; i < name_50_list.size(); i++)
	{
		vector<string> vNameTemp;
		vNameTemp.clear();
		vNameTemp = _get_name_25k_from_50k(name_50_list[i]);

		// 将vNameTemp中每个元素存储到name_25k_list中
		for (int j = 0; j < vNameTemp.size(); j++)
		{
			name_25k_list.push_back(vNameTemp[j]);
		}

	}
	return name_25k_list;
}

/*100万图幅名, 获取1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_10k_from_1m(string name_1m)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25_list;
	name_25_list.clear();

	name_25_list = _get_name_25k_from_1m(name_1m);

	for (int i = 0; i < name_25_list.size(); i++)
	{
		vector<string> vNameTemp;
		vNameTemp.clear();
		vNameTemp = _get_name_10k_from_25k(name_25_list[i]);

		// 将vNameTemp中每个元素存储到name_list中
		for (int j = 0; j < vNameTemp.size(); j++)
		{
			name_list.push_back(vNameTemp[j]);
		}

	}
	return name_list;
}

/*根据2.5万图幅名, 获取对应的1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_10k_from_25k(string name_25k)
{
	return _get_namelist_from_small_scale(name_25k);
}

/*100万图幅名, 获取1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_5k_from_1m(string name_1m)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25_list;
	name_25_list.clear();

	name_25_list = _get_name_25k_from_1m(name_1m);


	for (int i = 0; i < name_25_list.size(); i++)
	{
		vector<string> name_10k_list;
		name_10k_list.clear();
		name_10k_list = _get_name_10k_from_25k(name_25_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			vector<string> name_5k_list;
			name_5k_list.clear();
			name_5k_list = _get_name_5k_from_10k(name_10k_list[j]);
			for (int k = 0; k < name_5k_list.size(); k++)
			{
				name_list.push_back(name_5k_list[k]);
			}
		}
	}
	return name_list;

}

/*根据1万图幅名, 获取对应的5k图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_5k_from_10k(string name_10k)
{
	return _get_namelist_from_small_scale(name_10k);
}

/*获取50万图幅对应某比例尺的所有图幅列表*/
vector<string> CSE_MapSheet::_get_name_list_from_500k(string name_500k,double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_250K) < EQUAL_VALUE)
	{
		return _get_name_250k_from_500k(name_500k);		// 从50万得到25万名字
	}
	else if (fabs(child_scale - SCALE_100K) < EQUAL_VALUE)
	{
		return _get_name_100k_from_500k(name_500k);		// 从50万得到10万名字
	}
	else if (fabs(child_scale - SCALE_50K) < EQUAL_VALUE)
	{
		return _get_name_50k_from_500k(name_500k);		// 从50万得到5万名字
	}
	else if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)
	{
		return _get_name_25k_from_500k(name_500k);		// 从50万得到2.5万名字
	}
	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)
	{
		return _get_name_10k_from_500k(name_500k);		// 从50万得到1万名字
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)
	{
		return _get_name_5k_from_500k(name_500k);		// 从50万得到5k名字
	}
	else
	{
		return name_list;
	}
}

/*50万图幅名, 获取25万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_250k_from_500k(string name_500k)
{
	// 前n-1个字符
	string strTemp = name_500k.substr(0, name_500k.length() - 1);
	// 获取最后一个字符
	char theta_500k = name_500k.at(name_500k.length() - 1);

	vector<string> theta_250k_list;
	theta_250k_list.clear();

	if (theta_500k == '1')
	{
		theta_250k_list.push_back("01");
		theta_250k_list.push_back("02");
		theta_250k_list.push_back("05");
		theta_250k_list.push_back("06");
	}

	else if(theta_500k == '2')
	{
		theta_250k_list.push_back("03");
		theta_250k_list.push_back("04");
		theta_250k_list.push_back("07");
		theta_250k_list.push_back("08");
	}

	else if (theta_500k == '3')
	{
		theta_250k_list.push_back("09");
		theta_250k_list.push_back("10");
		theta_250k_list.push_back("13");
		theta_250k_list.push_back("14");
	}

	else
	{
		theta_250k_list.push_back("11");
		theta_250k_list.push_back("12");
		theta_250k_list.push_back("15");
		theta_250k_list.push_back("16");
	}

	vector<string> name_list;
	name_list.clear();
	for (int i = 0; i < theta_250k_list.size(); i++)
	{
		string strName = strTemp + theta_250k_list[i];
		name_list.push_back(strName);
	}

	return name_list;
}

/*50万图幅名, 获取10万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_100k_from_500k(string name_500k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_250k_list;
	name_250k_list.clear();

	name_250k_list = _get_name_250k_from_500k(name_500k);
	for (int i = 0; i < name_250k_list.size(); i++)
	{
		vector<string> name_100k_list;
		name_100k_list.clear();
		name_100k_list = _get_name_100k_from_250k(name_250k_list[i]);

		for (int j = 0; j < name_100k_list.size(); j++)
		{
			name_list.push_back(name_100k_list[j]);
		}

	}
	return name_list;

}

/*根据50万图幅名, 获取对应的5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_50k_from_500k(string name_500k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_100k_list;
	name_100k_list.clear();
	name_100k_list = _get_name_100k_from_500k(name_500k);
	
	for (int i = 0; i < name_100k_list.size(); i++)
	{
		vector<string> name_50k_list;
		name_50k_list.clear();
		name_50k_list = _get_name_50k_from_100k(name_100k_list[i]);

		for (int j = 0; j < name_50k_list.size(); j++)
		{
			name_list.push_back(name_50k_list[j]);
		}

	}
	return name_list;
}

/*根据50万图幅名, 获取对应的2.5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_25k_from_500k(string name_500k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_50k_list;
	name_50k_list.clear();
	name_50k_list = _get_name_50k_from_500k(name_500k);

	for (int i = 0; i < name_50k_list.size(); i++)
	{
		vector<string> name_25k_list;
		name_25k_list.clear();
		name_25k_list = _get_name_25k_from_50k(name_50k_list[i]);

		for (int j = 0; j < name_25k_list.size(); j++)
		{
			name_list.push_back(name_25k_list[j]);
		}

	}
	return name_list;
}

/*根据50万图幅名, 获取对应的1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_10k_from_500k(string name_500k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25k_list;
	name_25k_list.clear();
	name_25k_list = _get_name_25k_from_500k(name_500k);

	for (int i = 0; i < name_25k_list.size(); i++)
	{
		vector<string> name_10k_list;
		name_10k_list.clear();
		name_10k_list = _get_name_10k_from_25k(name_25k_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			name_list.push_back(name_10k_list[j]);
		}

	}
	return name_list;
}

/*根据50万图幅名, 获取对应的5k图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_5k_from_500k(string name_500k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25k_list;
	name_25k_list.clear();
	name_25k_list = _get_name_25k_from_500k(name_500k);

	for (int i = 0; i < name_25k_list.size(); i++)
	{
		vector<string> name_10k_list;
		name_10k_list.clear();
		name_10k_list = _get_name_10k_from_25k(name_25k_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			vector<string> name_5k_list;
			name_5k_list.clear();
			name_5k_list = _get_name_5k_from_10k(name_10k_list[j]);
			for (int k = 0; k < name_5k_list.size(); k++)
			{
				name_list.push_back(name_5k_list[k]);
			}
		}

	}
	return name_list;
}

/*获取25万图幅对应某比例尺的所有图幅列表*/
vector<string> CSE_MapSheet::_get_name_list_from_250k(string name_250k, double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_100K) < EQUAL_VALUE)
	{
		name_list = _get_name_100k_from_250k(name_250k);		// 从25万得到10万名字
	}
	
	else if(fabs(child_scale - SCALE_50K) < EQUAL_VALUE)
	{
		name_list = _get_name_50k_from_250k(name_250k);			// 从25万得到5万名字
	}

	else if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)
	{
		name_list = _get_name_25k_from_250k(name_250k);			// 从25万得到2.5万名字
	}

	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)
	{
		name_list = _get_name_10k_from_250k(name_250k);			// 从25万得到1万名字
	}

	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)
	{
		name_list = _get_name_5k_from_250k(name_250k);			// 从25万得到5k名字
	}

	else
	{
		return name_list;
	}

	return name_list;
}

/*25万图幅名, 获取10万图幅列表, 并返回*/
// [:-2]取前n-2个元素，此处为百万图幅编码
// [-2:]取数组的最后两个字符
vector<string> CSE_MapSheet::_get_name_100k_from_250k(string name_250k)
{
	vector<string> name_list;
	name_list.clear();
	
	// 取25万图幅的最后两个字符
	int	theta_250k = atoi(name_250k.substr(name_250k.length() - 2, 2).c_str());
	int N = 0;
	if (theta_250k % 4 == 0)
	{
		N = theta_250k / 4 - 1;
	}
	else
	{
		N = theta_250k / 4;
	}

	int column_1[3] = {0};
	column_1[0] = 1 + (theta_250k - 1) * 3 + 24 * N;
	column_1[1] = 2 + (theta_250k - 1) * 3 + 24 * N;
	column_1[2] = 3 + (theta_250k - 1) * 3 + 24 * N;

	int column_2[3] = {0};
	for (int i = 0; i < 3; i++)
	{
		column_2[i] = column_1[i] + 12;
	}

	int	column_3[3] = { 0 };
	for (int i = 0; i < 3; i++)
	{
		column_3[i] = column_1[i] + 12 * 2;
	}

	int column_2_new[6] = { 0 };
	column_2_new[0] = column_2[0];
	column_2_new[1] = column_2[1];
	column_2_new[2] = column_2[2];
	column_2_new[3] = column_3[0];
	column_2_new[4] = column_3[1];
	column_2_new[5] = column_3[2];

	int column_1_new[9] = { 0 };
	column_1_new[0] = column_1[0];
	column_1_new[1] = column_1[1];
	column_1_new[2] = column_1[2];
	column_1_new[3] = column_2_new[0];
	column_1_new[4] = column_2_new[1];
	column_1_new[5] = column_2_new[2];
	column_1_new[6] = column_2_new[3];
	column_1_new[7] = column_2_new[4];
	column_1_new[8] = column_2_new[5];

	for (int i = 0; i < 9; i++)
	{
		char szTheta[10] = { 0 };
		if (column_1_new[i] < 10)
		{
			sprintf(szTheta, "00%d", column_1_new[i]);
		}
		else if(column_1_new[i] < 100)
		{
			sprintf(szTheta, "0%d", column_1_new[i]);
		}
		else
		{
			sprintf(szTheta, "%d", column_1_new[i]);
		}
	
		string strTemp = name_250k.substr(name_250k.length() - 2, 2);
		string strTheta = szTheta;
		strTemp = strTemp + strTheta;
		name_list.push_back(strTemp);

	}
	return name_list;
}

/*获取10万图幅对应某比例尺的所有图幅列表*/
vector<string> CSE_MapSheet::_get_name_list_from_100k(string name_100k, double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_50K) < EQUAL_VALUE)		// 从10万得到5万名字
	{
		name_list = _get_name_50k_from_100k(name_100k);
	}
	else if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)	// 从10万得到2.5万名字
	{
		name_list = _get_name_25k_from_100k(name_100k);
	}
	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)	// 从10万得到1万名字
	{
		name_list = _get_name_10k_from_100k(name_100k);
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)	// 从10万得到5k名字
	{
		name_list = _get_name_5k_from_100k(name_100k);
	}
	else
	{
		return name_list;
	}

	return name_list;
}

/*获取5万图幅对应某比例尺的所有图幅列表*/
vector<string> CSE_MapSheet::_get_name_list_from_50k(string name_50k, double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)	// 从5万得到2.5万名字
	{
		name_list = _get_name_25k_from_50k(name_50k);
	}
	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)	// 从5万得到1万名字
	{
		name_list = _get_name_10k_from_50k(name_50k);
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)	// 从5万得到5k名字
	{
		name_list = _get_name_5k_from_50k(name_50k);
	}
	else
	{
		return name_list;
	}

	return name_list;
}

/*获取2.5万图幅对应某比例尺的所有图幅列表*/
vector<string> CSE_MapSheet::_get_name_list_from_25k(string name_25k, double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)	// 从2.5万得到1万名字
	{
		name_list = _get_name_10k_from_25k(name_25k);
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)	// 从2.5万得到5k名字
	{
		name_list = _get_name_5k_from_25k(name_25k);
	}
	else
	{
		return name_list;
	}

	return name_list;
}


/*	新增5k图幅范围，规范上没有该比例尺。
	获取1万图幅对应某比例尺的所有图幅列表*/
vector<string> CSE_MapSheet::_get_name_list_from_10k(string name_10k,double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)		// 从1万得到5k名字
	{
		name_list = _get_name_5k_from_10k(name_10k);
	}
	else
	{
		return name_list;
	}
	return name_list;
}

/*根据5万图幅名, 获取对应的2.5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_25k_from_50k(string name_50k)
{
	return _get_namelist_from_small_scale(name_50k);
}

/*根据10万图幅名, 获取对应的2.5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_25k_from_100k(string name_100k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_50k_list = _get_name_50k_from_100k(name_100k);

	for (int i = 0; i < name_50k_list.size(); i++)
	{
		vector<string> name_25k_list = _get_name_25k_from_50k(name_50k_list[i]);

		for (int j = 0; j < name_25k_list.size(); j++)
		{
			name_list.push_back(name_25k_list[j]);
		}
	}
	return name_list;
}


/*根据25万图幅名, 获取对应的5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_50k_from_250k(string name_250k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_100k_list = _get_name_100k_from_250k(name_250k);

	for (int i = 0; i < name_100k_list.size(); i++)
	{
		vector<string> name_50k_list = _get_name_50k_from_100k(name_100k_list[i]);

		for (int j = 0; j < name_50k_list.size(); j++)
		{
			name_list.push_back(name_50k_list[j]);
		}
	}
	return name_list;
}

/*根据25万图幅名, 获取对应的2.5万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_25k_from_250k(string name_250k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_50k_list = _get_name_50k_from_250k(name_250k);

	for (int i = 0; i < name_50k_list.size(); i++)
	{
		vector<string> name_25k_list = _get_name_25k_from_50k(name_50k_list[i]);

		for (int j = 0; j < name_25k_list.size(); j++)
		{
			name_list.push_back(name_25k_list[j]);
		}
	}
	return name_list;
}

/*根据25万图幅名, 获取对应的1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_10k_from_250k(string name_250k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25k_list = _get_name_25k_from_250k(name_250k);

	for (int i = 0; i < name_25k_list.size(); i++)
	{
		vector<string> name_10k_list = _get_name_10k_from_25k(name_25k_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			name_list.push_back(name_10k_list[j]);
		}
	}
	return name_list;
}

/*根据25万图幅名, 获取对应的1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_5k_from_250k(string name_250k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25k_list = _get_name_25k_from_250k(name_250k);

	for (int i = 0; i < name_25k_list.size(); i++)
	{
		vector<string> name_10k_list = _get_name_10k_from_25k(name_25k_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			vector<string> name_5k_list = _get_name_5k_from_10k(name_10k_list[j]);
			for (int k = 0; k < name_5k_list.size(); k++)
			{
				name_list.push_back(name_5k_list[k]);
			}
		}
	}
	return name_list;
}

/*根据10万图幅名, 获取对应的1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_10k_from_100k(string name_100k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25k_list = _get_name_25k_from_100k(name_100k);

	for (int i = 0; i < name_25k_list.size(); i++)
	{
		vector<string> name_10k_list = _get_name_10k_from_25k(name_25k_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			name_list.push_back(name_10k_list[j]);
		}
	}
	return name_list;
}


/*根据10万图幅名, 获取对应的5k图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_5k_from_100k(string name_100k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25k_list = _get_name_25k_from_100k(name_100k);

	for (int i = 0; i < name_25k_list.size(); i++)
	{
		vector<string> name_10k_list = _get_name_10k_from_25k(name_25k_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			vector<string> name_5k_list = _get_name_5k_from_10k(name_10k_list[j]);
			for (int k = 0; k < name_5k_list.size(); k++)
			{
				name_list.push_back(name_5k_list[k]);
			}
		}
	}
	return name_list;
}

/*根据5万图幅名, 获取对应的1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_10k_from_50k(string name_50k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25k_list = _get_name_25k_from_50k(name_50k);

	for (int i = 0; i < name_25k_list.size(); i++)
	{
		vector<string> name_10k_list = _get_name_10k_from_25k(name_25k_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			name_list.push_back(name_10k_list[j]);
		}
	}
	return name_list;
}

/*根据5万图幅名, 获取对应的5k图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_5k_from_50k(string name_50k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_25k_list = _get_name_25k_from_50k(name_50k);

	for (int i = 0; i < name_25k_list.size(); i++)
	{
		vector<string> name_10k_list = _get_name_10k_from_25k(name_25k_list[i]);

		for (int j = 0; j < name_10k_list.size(); j++)
		{
			vector<string> name_5k_list = _get_name_5k_from_10k(name_10k_list[j]);
			for (int k = 0; k < name_5k_list.size(); k++)
			{
				name_list.push_back(name_5k_list[k]);
			}
		}
	}
	return name_list;
}

/*根据2.5万图幅名, 获取对应的1万图幅列表, 并返回*/
vector<string> CSE_MapSheet::_get_name_5k_from_25k(string name_25k)
{
	vector<string> name_list;
	name_list.clear();

	vector<string> name_10k_list = _get_name_10k_from_25k(name_25k);

	for (int i = 0; i < name_10k_list.size(); i++)
	{
		vector<string> name_5k_list = _get_name_5k_from_10k(name_10k_list[i]);

		for (int j = 0; j < name_5k_list.size(); j++)
		{
			name_list.push_back(name_5k_list[j]);
		}
	}
	return name_list;
}


string CSE_MapSheet::_find_name_list(vector<string>& name_list, double lon, double lat)
{
	double lon_min;
	double lat_min;
	double lon_max;
	double lat_max;
	for (int i = 0; i < name_list.size(); i++)
	{
		get_box(name_list[i], lon_min, lat_max, lon_max, lat_min);
		if (lon_min > lon
			|| lat_min > lat
			|| lon_max < lon
			|| lat_max < lat)
		{
			continue;
		}
		else
		{
			return name_list[i];
		}
	}
	return "";
}

