#include "cse_mapsheet.h"
#define DOUBLE_EQUAL_ZERO 1e-6

CSE_MapSheet::CSE_MapSheet()
{

}

CSE_MapSheet::~CSE_MapSheet()
{

}

/*���ݾ�γ�ȷ�Χ(bbox)��ȡĳ������ͼ���б�*/
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

	// ȥ���߽緶Χ���ͼ���б�
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
	// gb2012תjb2008ͼ����
	// �����ͼ���Ͳ���"D"��"K"��"L"�����ؿ��ַ���
	if (map_type != "D"
		&& map_type != "K"
		&& map_type != "L")
	{
		return "";
	}

	if (map_type == "K")
	{
		// Kͼ��Ŀǰ��֧��500k��250k������ӦΪ10��11
		if (gb_name.length() > 11 || gb_name.length() < 10)
		{
			return "";
		}

		// �ж�ͼ���ŵ���λ�����λ�Ƿ�ΪB��C
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
		// ����Ϊ10��11��Lͼ������������λӦΪB��C
		if (gb_name.length() == 10 ||
			gb_name.length() == 11)
		{
			// �ж�ͼ���ŵ���λ�Ƿ�ΪB��C
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

	// ���ȷ�Χ[-180,180]��γ�ȷ�Χ[-90,90]
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

	// �������������ͼ���б�
	vector<string> vSheetList;
	vSheetList.clear();

	int lon_diff_1m_list[3] = { 6,12,24 };
	double lon_max = 0;
	while (lat_max > lat_min)
	{
		double lat = lat_max;
		// ����γ�����ڷ�������1��2��3����������[-60.0, 60.0)��[60,76) or [-76,-60)��[76,88] or [-88,-76]
		int lat_region = get_lat_region(lat - 4);
		if (lat_region == 0)
		{
			return "";
		}
		// ���Ȳ�
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

			// ��ȡ100������
			string name_1m = get_name_1m(lon * RAD8, lat * RAD8, map_type);  // ���Ͻ�
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
				// ��ȡ����ͼ����Ӧĳ�����ߵ�����ͼ���б�
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

	// ���˳���γ�ȵ����ڵ�ͼ����
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
	// ���㵱ǰͼ����γ�ȷ�Χ
	double dLeft = 0;
	double dTop = 0;
	double dRight = 0;
	double dBottom = 0;

	get_box(strSheet, dLeft, dTop, dRight, dBottom);

	// ���㾭�Ȳγ�Ȳ�����ϡ��¡�����ͼ�������ĵ�����
	double dWidth = dRight - dLeft;
	double dHeight = dTop - dBottom;

	// ��ǰͼ�����ĵ�����
	double dCenterPointX = dLeft + dWidth * 0.5;
	double dCenterPointY = dBottom + dHeight * 0.5;

	// ��ͼ�����ĵ�����
	double dTopCenterPointX = dCenterPointX;
	double dTopCenterPointY = dCenterPointY + dHeight;

	// ��ͼ�����ĵ�����
	double dBottomCenterPointX = dCenterPointX;
	double dBottomCenterPointY = dCenterPointY - dHeight;

	// ��ͼ�����ĵ�����
	double dLeftCenterPointX = dCenterPointX - dWidth;
	double dLeftCenterPointY = dCenterPointY;

	// ��ͼ�����ĵ�����
	double dRightCenterPointX = dCenterPointX + dWidth;
	double dRightCenterPointY = dCenterPointY;

	// �ֱ�����ϡ��¡��������ĵ�����ͼ����
	strTopSheet = get_name_from_latlon(scale, dTopCenterPointX, dTopCenterPointY, map_type);
	strBottomSheet = get_name_from_latlon(scale, dBottomCenterPointX, dBottomCenterPointY, map_type);
	strLeftSheet = get_name_from_latlon(scale, dLeftCenterPointX, dLeftCenterPointY, map_type);
	strRightSheet = get_name_from_latlon(scale, dRightCenterPointX, dRightCenterPointY, map_type);
}

string CSE_MapSheet::_gb2012_to_gjb2008_500k_250k_100k(string gb_name,string map_type)
{
	// 50��25��10������ߣ�2012�����ͼ������ת2008�����ͼ����
	//  jb_name = 'DN10501'         jb_name = 'DS10501'
	//	gb_name = 'J50B001001'      gb_name = 'SJ50B001001'
	// ��ȡ�����߱���
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

	// 50��
	if (strScaleCode == "B")
	{
		div_val = 2;
		seq_len = 1;
	}
	// 25��
	else if (strScaleCode == "C")
	{
		div_val = 4;
		seq_len = 2;
	}
	// 10��
	else if (strScaleCode == "D")
	{
		div_val = 12;
		seq_len = 3;
	}

	else
	{
		return "";
	}
	
	// �к�
	string strRowNum = gb_name.substr(gb_name.length() - 6, 3);

	// �к�
	string strColNum = gb_name.substr(gb_name.length() - 3, 3);

	int gb_row_num = atoi(strRowNum.c_str());
	int gb_column_num = atoi(strColNum.c_str());

	// 100��ͼ������
	string gb_1m_name;
	// ������
	if (gb_name.length() == 10)
	{
		gb_1m_name = gb_name.substr(0, 3);
	}
	// �ϰ���
	else if (gb_name.length() == 11)
	{
		gb_1m_name = gb_name.substr(1, 3);
	}
	
	string jb_1m_name = _gb2012_to_gjb2008_1m(gb_1m_name, map_type);
	int jb_seq = (gb_row_num - 1) * div_val + gb_column_num;

	// �������
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
	/*5��2.5��1������ߣ�2012�����ͼ������ת2008�����ͼ����,
	�������ĵ�50k_25k_10k��ת����ʽ��ȫ���ú�����50k_25k_10k��ת����ʽ�������ģ�*/
	
	// ��ȡ�����߱���
	string strScaleCode;
	if (gb_name.length() == 10)
	{
		strScaleCode = gb_name.substr(3, 1);
	}
	else if (gb_name.length() == 11)
	{
		strScaleCode = gb_name.substr(4, 1);
	}

	// ������һ������
	string father_gb_type;
	// 5��
	if (strScaleCode == "E")
	{
		father_gb_type = "D";
	}
	// 2.5��
	else if (strScaleCode == "F")
	{
		father_gb_type = "E";
	}
	// 1��
	else if (strScaleCode == "G")
	{
		father_gb_type = "F";
	}
	// 5ǧ
	else if (strScaleCode == "H")
	{
		father_gb_type = "G";
	}
	else
	{
		return "";
	}

	// �к�
	string strRowNum = gb_name.substr(gb_name.length() - 6, 3);

	// �к�
	string strColNum = gb_name.substr(gb_name.length() - 3, 3);

	int gb_row_num = atoi(strRowNum.c_str());
	int gb_column_num = atoi(strColNum.c_str());

	// ����100��ͼ������
	string gb_1m_name;
	// ������
	if (gb_name.length() == 10)
	{
		gb_1m_name = gb_name.substr(0, 3);
	}
	// �ϰ���
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
	
	// �б�����"0"�ĸ���
	string str_father_gb_row_num = to_string(father_gb_row_num);
	size_t iRowZeroCount = 3 - str_father_gb_row_num.length();
	for (int i = 0; i < iRowZeroCount; i++)
	{
		str_father_gb_row_num = "0" + str_father_gb_row_num;
	}

	// �б�����"0"�ĸ���
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
	// 100������ߣ�2012�����ͼ������ת2008�����ͼ����
	// jb_name = 'DN1047'  jb_name = 'DS1047'
	// gb_name = 'G47'     gb_name = 'SG47'
	
	string str_gb_row;
	string gb_column_num;
	
		// ������
	if (gb_name.length() == 3)
	{
		str_gb_row = gb_name.substr(0, 1);
		gb_column_num = gb_name.substr(1, 2);
	}
	// �ϰ���
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

	// �к�����
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

	// ����ͼ����
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
	// �ϰ���
	if (gb_name.length() > 3
		&& strFlag == "S")
	{
		jb_name = map_type + "S" + jb_name;
	}
	// ������
	else
	{
		jb_name = map_type + "N" + jb_name;
	}

	return jb_name;
}

/*	���ݾ�γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]����ȡ����ͼ���µ�����ĳ������ͼ���б�,
	�����ط�Χ�ڼ���߽紦ͼ���б�Ĭ�Ϸ�Χ[-180, -88, 180, 88]*/
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

	// ���γ�ȡ�4��������
	if (int(lat_max) % 4 > EQUAL_VALUE)
	{
		lat_max = (int(lat_max) / 4 + 1) * 4;
	}

	int lon_diff_1m_list[3] = { 6,12,24 };
	while (lat_max > lat_min)
	{
		double lat = lat_max;
		// ����γ�����ڷ�������1��2��3����������[-60.0, 60.0)��[60,76) or [-76,-60)��[76,88] or [-88,-76]
		int lat_region = get_lat_region(lat - 4);
		if (lat_region == 0)
		{
			return;
		}
		// ���Ȳ�
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

			// ��ȡ100������
			string name_1m = get_name_1m(lon * RAD8, lat * RAD8, map_type);  // ���Ͻ�

			// ��ȡ����ͼ����Ӧĳ�����ߵ�����ͼ���б�
			if (fabs(scale - SCALE_1M) < DOUBLE_EQUAL_ZERO)
			{
				name_list.push_back(name_1m);
			}
			else
			{
				name_list = get_large_scale_name_list(name_1m, SCALE_1M, scale);
			}			

			// ��Χ�ںͱ߽紦ͼ���ֿ��浽������
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

/*ɾ��������Χ�߽����ͼ����, �����ر߽紦���ڷ�Χ�ڵ�ͼ���б�
	: param bbox : ������γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max], Ĭ��[-180, -88, 180, 88]
	: param name_outer : ������Χ��ͼ���б�
	:return : �߽紦���ڷ�Χ�ڵ�ͼ���б�*/
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
		// ������ͼ����Χ�ڵ�ͼ����ȥ��
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
	// ����ͼ�����е�N��S�������DN��DS��ͷ��ͼ���ţ���ȥ��D
	size_t iIndex = strCode.find('N');
	string strCalCode;		// ������ͼ���ŵı���
	if (iIndex != -1)		// ����ҵ�N����˵���Ǳ��������
	{
		if (iIndex != 0)	// �����λ����0������Ҫ��Nǰ���Dȥ��
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}
	else    // ˵�����ϰ���
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

	// �����100������ߣ�N1050��
	if (strCalCode.length() == 5)
	{
		return 1000000;
	}
	// �����50�������
	else if (strCalCode.length() == 6)
	{
		return 500000;
	}
	// �����25�������
	else if (strCalCode.length() == 7)
	{
		return 250000;
	}
	// �����10�������
	else if (strCalCode.length() == 8)
	{
		return 100000;
	}
	// �����5�������
	else if (strCalCode.length() == 9)
	{
		return 50000;
	}
	// �����2.5�������
	else if (strCalCode.length() == 10)
	{
		return 25000;
	}
	// �����1�������
	else if (strCalCode.length() == 11)
	{
		return 10000;
	}
	// �����5ǧ������
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
����2008��jbͼ�������㾭γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]

��֧�ֵ���ͼD 10k - 5k(����1:5000, 11λ)������ͼK 250k - 500k(1m��2m��ʱ��֧��)��������սͼL 250k - 1m
��ʱ��֧�ֺ���ͼ1m��2m
��֧�ֺ���ͼ��ר��ͼ

ͼ����ȫ������ʱ��Ĭ�ϱ��������ͼ
	: param name : ͼ����
	:return : ͼ����γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]
*/

bool CSE_MapSheet::get_box(string strCode, double& left, double& top, double& right, double& bottom)
{
	//  [6/5/2022 pippo]
	// ����400�������ͼ������
	/*��100��ַ�����Ϊ������������180�����㣬����Ա������ϣ���N/S��ʶ������400W�ַ����֣���ȫ�򻮷�Ϊ180��400Wͼ�����ڸ�γ�ȵ���80��~88�㣬400Wͼ��ֻ����8������ͼ����Χ��
		ͼ����Χ��ÿ16����������Ϊһ��400��ͼ����
		������ʽ����������Ϊ10���ַ�
		ǰ��λ�����ͼ��һ�£�L��ʾLHZZT��N��S��ʾ��γ����γ��
		��3λ����6λΪγ�ȷ�����С����������ϣ�
		��7λ����10λΪ���ȷ�����С����������ϣ�
	*/
	// �����400w������
	if ((strCode.find("LN") != -1 || strCode.find("LS") != -1)
		&& strCode.length() == 10)
	{
		// γ����С���룬3��4λ
		int iLatFromCode = atoi(strCode.substr(2, 2).c_str());

		// γ�������룬5��6λ
		int iLatToCode = atoi(strCode.substr(4, 2).c_str());

		// ������С���룬7��8λ
		int iLonFromCode = atoi(strCode.substr(6, 2).c_str());

		// ���������룬9��10λ
		int iLonToCode = atoi(strCode.substr(8, 2).c_str());

		size_t iIndexOfN = strCode.find('N');
		// ����ҵ�N����˵���Ǳ��������
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

	// ����ͼ�����е�N��S�������DN��DS��ͷ��ͼ���ţ���ȥ��D
	size_t iIndex = strCode.find('N');
	string strCalCode;		// ������ͼ���ŵı���
	if (iIndex != -1)		// ����ҵ�N����˵���Ǳ��������
	{
		if (iIndex != 0)	// �����λ����0������Ҫ��Nǰ���Dȥ��
		{
			strCalCode = strCode.substr(iIndex, strCode.length());
		}
		else
		{
			strCalCode = strCode;
		}
	}
	else    // ˵�����ϰ���
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

	// �����100������ߣ�N1050��
	if (strCalCode.length() == 5)
	{
		return get_box_1m(strCalCode, left, top, right, bottom);
	}
	// �����50�������
	else if (strCalCode.length() == 6)
	{
		return get_box_500k_250k_100k_2008(strCalCode, 500000, left, top, right, bottom);
	}
	// �����25�������
	else if (strCalCode.length() == 7)
	{
		return get_box_500k_250k_100k_2008(strCalCode, 250000, left, top, right, bottom);
	}
	// �����10�������
	else if (strCalCode.length() == 8)
	{
		return get_box_500k_250k_100k_2008(strCalCode, 100000, left, top, right, bottom);
	}
	// �����5�������
	else if (strCalCode.length() == 9)
	{
		return get_bbox_50k_2008(strCalCode, 50000, left, top, right, bottom);
	}
	// �����2.5�������
	else if (strCalCode.length() == 10)
	{
		return get_bbox_25k_2008(strCalCode, 25000, left, top, right, bottom);
	}
	// �����1�������
	else if (strCalCode.length() == 11)
	{
		return get_bbox_10k_2008(strCalCode, 10000, left, top, right, bottom);
	}
	// �����5ǧ������
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

// ��ȡ����ͼ����γ�ȱ߽磨N1050��
bool CSE_MapSheet::_lat_1m(string strCode, double& dLatMax, double& dLatMin)
{
	string strRow = strCode.substr(1, 2);
	int iRow = atoi(strRow.c_str());

	double latMax = 4 * iRow;
	double latMin = 4 * (iRow - 1);

	// �ϰ���
	if (strCode.length() > 4 && strCode.find('S') != -1)
	{
		dLatMax = -latMin;
		dLatMin = -latMax;
	}
	// ������
	else
	{
		dLatMax = latMax;
		dLatMin = latMin;
	}
	return true;
}


// ��ȡ����ͼ����γ�ȱ߽磨N1050��
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
	else  // ͼ�������Ϸ�:ǰ��λ����Ӧ<=22
	{
		return false;
	}

	return true;
}

// ��ȡ50��25��10��ͼ����Χ
bool CSE_MapSheet::get_box_500k_250k_100k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*
	def _bbox_500k_250k_100k_2008(name, scale) :
	"""
	����50��25��10��ͼ������γ�Ȳ���Ȳ������������������2008��50��25��10��γ�ȷ�Χ
	: param name : ͼ����
	:param lat_diff : ͼ��γ�Ȳ�
	:param lon_diff : ͼ�����Ȳ�
	:param row_sum : ͼ���Ӹ����ָܷ�ɵ���������������
	:return : ͼ����γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]
	"""*/

	// 100��ͼ������
	string str1mCode = strCode.substr(0, 5);

	// ��ȡ100��ͼ���ľ�γ�ȷ�Χ
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

	int sequence_num = atoi(strCode.substr(5, strCode.length()).c_str());        //int(name[5:])   # ͼ����ȥ������ͼ��������к�
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(dLonMin, dLatMin, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

int CSE_MapSheet::_get_row_nums(double scale)
{
	/* 	���ݱ����߼���ַ�������
	������������ͬ
		���磬100���Ϊ50��2��2��
		: param scale : ������
		:return : �ַ�����*/
		/*# ����ͼ�������ߴӸ����õ��ķַ�����, 10��500���ͼ��
			ROW_COLUMN_NUM_DIC = { SCALE_500K: 2,
			SCALE_250K : 4,
			SCALE_100K : 12,
			SCALE_50K : 2,
			SCALE_25K : 2,
			SCALE_10K : 2,
			SCALE_5K : 2
		}*/

		// 50�������
	if (fabs(scale - 500000) < 1e-6)
	{
		return 2;
	}
	// 25�������
	else if (fabs(scale - 250000) < 1e-6)
	{
		return 4;
	}
	// 10�������
	else if (fabs(scale - 100000) < 1e-6)
	{
		return 12;
	}
	// 5�������
	else if (fabs(scale - 50000) < 1e-6)
	{
		return 2;
	}
	// 2.5�������
	else if (fabs(scale - 25000) < 1e-6)
	{
		return 2;
	}
	// 1�������
	else if (fabs(scale - 10000) < 1e-6)
	{
		return 2;
	}
	// 5k������
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
	/*������ź��������������������������������к�
		: param sequence_num : ���
		:param row_sum : ������������������
		:return : �кź��к� row, column*/

	if (sequence_num > row_sum)
	{
		// ��:row������:colume
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
	��ͼ����γ�ȷ�Χ
	: param lon_left : ͼ������100�����½Ǿ���
	:param lat_bottom : ͼ������100�����½�γ��
	:param scale : ��ǰͼ��������
	:param row : �к�
	:param column : �к�
	:param row_sum : ��������������
	:return : ͼ����γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]*/

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
	���ݱ����߼���γ�Ȳ�

	: param scale : ������
	:return : γ�Ȳ�*/

	//return lat_diff_dic[scale]
	/*# ����ͼ��������γ�Ȳÿ���8��
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
	/*���ݱ����߼���������ľ��Ȳ�

	: param scale : ������
	:return : ���Ȳ�*/
	/*# ����ͼ�������߲�ͬγ�����򾭶Ȳÿ���8
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
	/*���ݱ����߼���������ľ��Ȳ�

	: param scale : ������
	:return : ���Ȳ�*/
	/*# ����ͼ�������߲�ͬγ�����򾭶Ȳÿ���8
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

// ��ȡ5��ͼ����Χ
bool CSE_MapSheet::get_bbox_50k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*����ͼ��������2008��5��ͼ����Χ
	: param name : ͼ����
	:param scale : ������
	:return : ͼ����γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]*/
	string name_100k = strCode.substr(0, strCode.length() - 1);		// ȡ5��ͼ�������ǰn-1λ

	double d100k_left = 0;
	double d100k_top = 0;
	double d100k_right = 0;
	double d100k_bottom = 0;

	// ��ȡ10wͼ���ķ�Χ
	get_box_500k_250k_100k_2008(name_100k, 100000, d100k_left, d100k_top, d100k_right, d100k_bottom);
	int sequence_num = atoi(strCode.substr(8, 1).c_str());	// ͼ����ȥ��10��ͼ��������к�
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d100k_left, d100k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}


// ��ȡ2.5��ͼ����Χ
bool CSE_MapSheet::get_bbox_25k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*����ͼ��������2008��2.5��ͼ����Χ
	: param name : ͼ����
	:param scale : ������
	:return : ͼ����γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]*/
	string name_50k = strCode.substr(0, strCode.length() - 1);		// ȡ2.5��ͼ�������ǰn-1λ

	double d50k_left = 0;
	double d50k_top = 0;
	double d50k_right = 0;
	double d50k_bottom = 0;

	// ��ȡ5wͼ���ķ�Χ
	get_bbox_50k_2008(name_50k, 50000, d50k_left, d50k_top, d50k_right, d50k_bottom);
	int sequence_num = atoi(strCode.substr(9, 1).c_str());	// ͼ����ȥ��5��ͼ��������к�
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d50k_left, d50k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

// ��ȡ1��ͼ����Χ
bool CSE_MapSheet::get_bbox_10k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*����ͼ��������2008��1��ͼ����Χ
	: param name : ͼ����
	:param scale : ������
	:return : ͼ����γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]*/
	string name_25k = strCode.substr(0, strCode.length() - 1);		// ȡ1��ͼ�������ǰn-1λ

	double d25k_left = 0;
	double d25k_top = 0;
	double d25k_right = 0;
	double d25k_bottom = 0;

	// ��ȡ2.5wͼ���ķ�Χ
	get_bbox_25k_2008(name_25k, 25000, d25k_left, d25k_top, d25k_right, d25k_bottom);
	int sequence_num = atoi(strCode.substr(10, 1).c_str());	// ͼ����ȥ��2.5��ͼ��������к�
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d25k_left, d25k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

// ��ȡ5000ͼ����Χ
bool CSE_MapSheet::get_bbox_5k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom)
{
	/*����ͼ��������2008��5kͼ����Χ
	: param name : ͼ����
	:param scale : ������
	:return : ͼ����γ�ȷ�Χ[lon_min, lat_min, lon_max, lat_max]*/
	string name_10k = strCode.substr(0, strCode.length() - 1);		// ȡ5kͼ�������ǰn-1λ

	double d10k_left = 0;
	double d10k_top = 0;
	double d10k_right = 0;
	double d10k_bottom = 0;

	// ��ȡ1wͼ���ķ�Χ
	get_bbox_10k_2008(name_10k, 10000, d10k_left, d10k_top, d10k_right, d10k_bottom);
	int sequence_num = atoi(strCode.substr(11, 1).c_str());	// ͼ����ȥ��1��ͼ��������к�
	int row_sum = _get_row_nums(dScale);
	int row = 0;
	int column = 0;
	_calculate_row_column(sequence_num, row_sum, row, column);
	_calculate_bbox(d10k_left, d10k_bottom, dScale, row, column, row_sum, left, bottom, right, top);
	return true;
}

/*����γ�ȼ���γ�ȷ���
	: param lat : γ��
	:return : γ�ȷ���*/
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

/*��ȡ100�������ͼ����
	: param map_type : ��ͼ���ͣ�"D"��ʾ����ͼ
	: param lon_start : ���ϽǾ���, ���� lon * 60 * 60 * 8
	: param lat_start : ���Ͻ�γ��, ���� lat * 60 * 60 * 8

	: return : 100�������ͼ����*/
string CSE_MapSheet::get_name_1m( double lon_start, double lat_start,string map_type)
{
	double lat_diff = get_lat_diff(SCALE_1M);			// γ�Ȳ� * 60 * 60 * 8
	double lat_region = get_lat_region(double(lat_start - lat_diff) / RAD8);  // γ�ȷ���
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

/*����100��������кţ���ת���8�ټ���
	lat, ��lon�������Ͻǣ�

	����Ҫ�����½�*/
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

/*����100��������кţ���ת���8�ټ���*/
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

/*	����С������ͼ��������������ͼ�����б�

	:param name:��ǰͼ����
	:param scale:��ǰͼ���ı����ߣ���Χ��1��100��(����5kͼ����Χ���淶��û�иñ����ߡ�)
	:param large_scale:������߷�ĸ
	:return:�������ͼ�����б�*/
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

	// �ж�����ĸ
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
	//��ȡ����ͼ����Ӧĳ�����ߵ�����ͼ���б�
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


	
/*���ݰ���ͼ����, ��ȡ��Ӧ��50��ͼ���б�, ������*/
vector<string> CSE_MapSheet::_get_name_500k_from_1m(string name_1m)
{
	return _get_namelist_from_small_scale(name_1m);
}

/*������һ��ͼ����, �ֳ�4��, ��ȡ��һ��ͼ���б�, ������

	100��-- > 50��
	10��-- > 5��
	5��-- > 2.5��
	2.5��-- > 1��
	1��-- > 5k*/
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

/*100��ͼ����, ��ȡ25��ͼ���б�, ������*/
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


/*100��ͼ����, ��ȡ10��ͼ���б�, ������*/
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

/*100��ͼ����, ��ȡ5��ͼ���б�, ������*/
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

		// ��vNameTemp��ÿ��Ԫ�ش洢��name_50k_list��
		for (int j = 0; j < vNameTemp.size(); j++)
		{
			name_50k_list.push_back(vNameTemp[j]);
		}

	}

	return name_50k_list;
}

/*����10��ͼ����, ��ȡ��Ӧ��5��ͼ���б�, ������*/
vector<string> CSE_MapSheet::_get_name_50k_from_100k(string name_100k)
{
	return _get_namelist_from_small_scale(name_100k);
}


/*100��ͼ����, ��ȡ2.5��ͼ���б�, ������*/
vector<string> CSE_MapSheet::_get_name_25k_from_1m(string name_1m)
{
	// ���ؽ��
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

		// ��vNameTemp��ÿ��Ԫ�ش洢��name_25k_list��
		for (int j = 0; j < vNameTemp.size(); j++)
		{
			name_25k_list.push_back(vNameTemp[j]);
		}

	}
	return name_25k_list;
}

/*100��ͼ����, ��ȡ1��ͼ���б�, ������*/
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

		// ��vNameTemp��ÿ��Ԫ�ش洢��name_list��
		for (int j = 0; j < vNameTemp.size(); j++)
		{
			name_list.push_back(vNameTemp[j]);
		}

	}
	return name_list;
}

/*����2.5��ͼ����, ��ȡ��Ӧ��1��ͼ���б�, ������*/
vector<string> CSE_MapSheet::_get_name_10k_from_25k(string name_25k)
{
	return _get_namelist_from_small_scale(name_25k);
}

/*100��ͼ����, ��ȡ1��ͼ���б�, ������*/
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

/*����1��ͼ����, ��ȡ��Ӧ��5kͼ���б�, ������*/
vector<string> CSE_MapSheet::_get_name_5k_from_10k(string name_10k)
{
	return _get_namelist_from_small_scale(name_10k);
}

/*��ȡ50��ͼ����Ӧĳ�����ߵ�����ͼ���б�*/
vector<string> CSE_MapSheet::_get_name_list_from_500k(string name_500k,double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_250K) < EQUAL_VALUE)
	{
		return _get_name_250k_from_500k(name_500k);		// ��50��õ�25������
	}
	else if (fabs(child_scale - SCALE_100K) < EQUAL_VALUE)
	{
		return _get_name_100k_from_500k(name_500k);		// ��50��õ�10������
	}
	else if (fabs(child_scale - SCALE_50K) < EQUAL_VALUE)
	{
		return _get_name_50k_from_500k(name_500k);		// ��50��õ�5������
	}
	else if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)
	{
		return _get_name_25k_from_500k(name_500k);		// ��50��õ�2.5������
	}
	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)
	{
		return _get_name_10k_from_500k(name_500k);		// ��50��õ�1������
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)
	{
		return _get_name_5k_from_500k(name_500k);		// ��50��õ�5k����
	}
	else
	{
		return name_list;
	}
}

/*50��ͼ����, ��ȡ25��ͼ���б�, ������*/
vector<string> CSE_MapSheet::_get_name_250k_from_500k(string name_500k)
{
	// ǰn-1���ַ�
	string strTemp = name_500k.substr(0, name_500k.length() - 1);
	// ��ȡ���һ���ַ�
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

/*50��ͼ����, ��ȡ10��ͼ���б�, ������*/
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

/*����50��ͼ����, ��ȡ��Ӧ��5��ͼ���б�, ������*/
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

/*����50��ͼ����, ��ȡ��Ӧ��2.5��ͼ���б�, ������*/
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

/*����50��ͼ����, ��ȡ��Ӧ��1��ͼ���б�, ������*/
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

/*����50��ͼ����, ��ȡ��Ӧ��5kͼ���б�, ������*/
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

/*��ȡ25��ͼ����Ӧĳ�����ߵ�����ͼ���б�*/
vector<string> CSE_MapSheet::_get_name_list_from_250k(string name_250k, double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_100K) < EQUAL_VALUE)
	{
		name_list = _get_name_100k_from_250k(name_250k);		// ��25��õ�10������
	}
	
	else if(fabs(child_scale - SCALE_50K) < EQUAL_VALUE)
	{
		name_list = _get_name_50k_from_250k(name_250k);			// ��25��õ�5������
	}

	else if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)
	{
		name_list = _get_name_25k_from_250k(name_250k);			// ��25��õ�2.5������
	}

	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)
	{
		name_list = _get_name_10k_from_250k(name_250k);			// ��25��õ�1������
	}

	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)
	{
		name_list = _get_name_5k_from_250k(name_250k);			// ��25��õ�5k����
	}

	else
	{
		return name_list;
	}

	return name_list;
}

/*25��ͼ����, ��ȡ10��ͼ���б�, ������*/
// [:-2]ȡǰn-2��Ԫ�أ��˴�Ϊ����ͼ������
// [-2:]ȡ�������������ַ�
vector<string> CSE_MapSheet::_get_name_100k_from_250k(string name_250k)
{
	vector<string> name_list;
	name_list.clear();
	
	// ȡ25��ͼ������������ַ�
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

/*��ȡ10��ͼ����Ӧĳ�����ߵ�����ͼ���б�*/
vector<string> CSE_MapSheet::_get_name_list_from_100k(string name_100k, double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_50K) < EQUAL_VALUE)		// ��10��õ�5������
	{
		name_list = _get_name_50k_from_100k(name_100k);
	}
	else if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)	// ��10��õ�2.5������
	{
		name_list = _get_name_25k_from_100k(name_100k);
	}
	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)	// ��10��õ�1������
	{
		name_list = _get_name_10k_from_100k(name_100k);
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)	// ��10��õ�5k����
	{
		name_list = _get_name_5k_from_100k(name_100k);
	}
	else
	{
		return name_list;
	}

	return name_list;
}

/*��ȡ5��ͼ����Ӧĳ�����ߵ�����ͼ���б�*/
vector<string> CSE_MapSheet::_get_name_list_from_50k(string name_50k, double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_25K) < EQUAL_VALUE)	// ��5��õ�2.5������
	{
		name_list = _get_name_25k_from_50k(name_50k);
	}
	else if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)	// ��5��õ�1������
	{
		name_list = _get_name_10k_from_50k(name_50k);
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)	// ��5��õ�5k����
	{
		name_list = _get_name_5k_from_50k(name_50k);
	}
	else
	{
		return name_list;
	}

	return name_list;
}

/*��ȡ2.5��ͼ����Ӧĳ�����ߵ�����ͼ���б�*/
vector<string> CSE_MapSheet::_get_name_list_from_25k(string name_25k, double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_10K) < EQUAL_VALUE)	// ��2.5��õ�1������
	{
		name_list = _get_name_10k_from_25k(name_25k);
	}
	else if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)	// ��2.5��õ�5k����
	{
		name_list = _get_name_5k_from_25k(name_25k);
	}
	else
	{
		return name_list;
	}

	return name_list;
}


/*	����5kͼ����Χ���淶��û�иñ����ߡ�
	��ȡ1��ͼ����Ӧĳ�����ߵ�����ͼ���б�*/
vector<string> CSE_MapSheet::_get_name_list_from_10k(string name_10k,double child_scale)
{
	vector<string> name_list;
	name_list.clear();

	if (fabs(child_scale - SCALE_5K) < EQUAL_VALUE)		// ��1��õ�5k����
	{
		name_list = _get_name_5k_from_10k(name_10k);
	}
	else
	{
		return name_list;
	}
	return name_list;
}

/*����5��ͼ����, ��ȡ��Ӧ��2.5��ͼ���б�, ������*/
vector<string> CSE_MapSheet::_get_name_25k_from_50k(string name_50k)
{
	return _get_namelist_from_small_scale(name_50k);
}

/*����10��ͼ����, ��ȡ��Ӧ��2.5��ͼ���б�, ������*/
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


/*����25��ͼ����, ��ȡ��Ӧ��5��ͼ���б�, ������*/
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

/*����25��ͼ����, ��ȡ��Ӧ��2.5��ͼ���б�, ������*/
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

/*����25��ͼ����, ��ȡ��Ӧ��1��ͼ���б�, ������*/
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

/*����25��ͼ����, ��ȡ��Ӧ��1��ͼ���б�, ������*/
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

/*����10��ͼ����, ��ȡ��Ӧ��1��ͼ���б�, ������*/
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


/*����10��ͼ����, ��ȡ��Ӧ��5kͼ���б�, ������*/
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

/*����5��ͼ����, ��ȡ��Ӧ��1��ͼ���б�, ������*/
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

/*����5��ͼ����, ��ȡ��Ӧ��5kͼ���б�, ������*/
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

/*����2.5��ͼ����, ��ȡ��Ӧ��1��ͼ���б�, ������*/
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

