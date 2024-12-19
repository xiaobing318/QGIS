#ifndef CONSISTENCY_PROCESSING_BASE_H
#define CONSISTENCY_PROCESSING_BASE_H
/**********************GDAL***************************/

//  这个类提供了对各种矢量数据格式（包括Shapefile）的基本操作接口，如访问数据源、读写图层等
#include <ogrsf_frmts.h>
//  提供了对单个矢量数据要素的表示和操作，包括获取和设置属性（字段值）、几何形状等
#include <ogr_feature.h>

/**********************自定义头文件***************************/
#include <commontype/se_commondef.h>
#include <vector/cse_vector_feature_matching.h>

/*
矢量数据一致性匹配工具
一、分析
-----------------------------------------------------------------穷举比较法------------------------------------------------------------------
1. 效率不高的空间匹配算法：穷举比较法
**算法思路：**
- **读取图层**：首先，使用GDAL读取两个shp图层，将每个图层中的要素（通常是几何对象，如多边形、点或线）加载到内存中。
- **循环比较**：对第一个图层中的每个要素，遍历第二个图层中的所有要素，使用几何比较操作（如`Intersects`或`Contains`）来检查它们是否空间上匹配。
- **存储结果**：对于每个匹配的要素对，记录下来，可能是添加到一个列表或者新的shp图层中。

**缺点**：
- 此方法的时间复杂度接近O(n*m)，其中n和m分别是两个图层的要素数量。对于大数据集，这种方法可能非常慢。
-----------------------------------------------------------------穷举比较法------------------------------------------------------------------

-----------------------------------------------------------------空间索引和查询--------------------------------------------------------------
2. 效率高的空间匹配算法：空间索引和查询
**算法思路：**
- **构建空间索引**：首先，对其中一个图层（例如较大或较复杂的图层）的要素构建空间索引，如R树（R-tree）或四叉树（Quadtree）。GDAL本身不直接支持
空间索引的创建，但可以使用外部库如libspatialindex。
- **空间查询**：对于另一个图层中的每个要素，使用空间索引进行高效的查询，找出可能的匹配要素。
- **精确匹配**：对索引返回的要素进行精确的空间匹配检查，以确认它们是否真正匹配。
- **存储结果**：将匹配的要素对存储下来。

**优点**：
- 使用空间索引可以显著降低查询时间，通常可将时间复杂度降到O(n log m)或更好，取决于索引的效率和数据的分布。
-----------------------------------------------------------------空间索引和查询---------------------------------------------------------------


二、矢量数据类型
  1、单点
  2、单线
  3、单面
  4、多部件

三、匹配算法
  1、双缓冲算法
  2、其他

四、算法流程
  1、算法一
    (1)得到两个图层
    (2)两层循环，外层循环是本底数据图层中的第i个要素，内层循环是匹配数据图层中的第j个要素
    (3)拿到两个图层中要素进行空间匹配（对两个要素进行的具体空间匹配算法）、属性匹配（对两个要素进行的具体属性匹配算法）
    (4)输出匹配之后的结果
  
  2、算法二（需要进行优化的内容）

*/

namespace consistency_processing_base
{

  typedef struct post_process_input_arguments
  {
    std::string threshold_level1;
    std::string threshold_level2;
    std::string threshold_level3;
    std::string threshold_level4;
    OGRFeature* matched_single_feature;
    OGRFeature* match_single_feature;
    post_process_input_arguments()
    {
      threshold_level1 = "";
      threshold_level2 = "";
      threshold_level3 = "";
      threshold_level4 = "";
      matched_single_feature = nullptr;
      match_single_feature = nullptr;
    }
  }post_process_input_arguments_t;

  typedef struct matching_form
  {
    //  匹配表单中需要记录的内容
    //  SMD （空间匹配度）
    std::string SMD;
    //  Attr_Sim （属性相似度）
    std::string Attr_Sim;
    //  SameEleConf （同名要素确认度）
    std::string SameEleConf;
    //  RelFacID （关联要素ID ）
    std::string RelFacID;
    //  Data1_ID(数据集1)
    std::string Data1_ID;
    std::string TCStatus;
    matching_form()
    {
      SMD = "";
      Attr_Sim = "";
      SameEleConf = "";
      RelFacID = "";
      Data1_ID = "";
      TCStatus = "";
    }
  }matching_form_t;

  typedef struct thresholds
  {
    std::string threshold_level1;
    std::string threshold_level2;
    std::string threshold_level3;
    std::string threshold_level4;
    thresholds()
    {
      threshold_level1 = "";
      threshold_level2 = "";
      threshold_level3 = "";
      threshold_level4 = "";
    }
  }thresholds_t;
}

class ogr_consistency_matching
{
//  public函数成员
public:
  //  构造函数和析构函数
  ogr_consistency_matching(OGRLayer* matched_layer, OGRLayer* match_layer, consistency_processing_base::thresholds_t& thresholds);
  ~ogr_consistency_matching();

  OGRLayer* get_m_matched_layer();
  OGRLayer* get_m_match_layer();
  void set_m_matched_layer(OGRLayer* layer);
  void set_m_match_layer(OGRLayer* layer);

  //  对两个矢量图层中的要素进行一致性匹配
  SE_Error two_ogr_layer_consistency_matching(
    std::vector<consistency_processing_base::matching_form_t>& vmatching_form);

//  private函数成员
private:
  //  对两个矢量图层中的一对要素进行一致性匹配
  SE_Error single_feature_match_single_feature(
    const OGRFeature* matched_single_feature, 
    const OGRFeature* match_single_feature,
    bool& flag);
  
//  private数据成员
private:
  //  本底数据图层
  OGRLayer* m_matched_layer = nullptr;
  //  匹配数据图层
  OGRLayer* m_match_layer = nullptr;
  //  阈值等级
  consistency_processing_base::thresholds_t m_thresholds = consistency_processing_base::thresholds_t();
};


#endif //CONSISTENCY_PROCESSING_BASE_H
