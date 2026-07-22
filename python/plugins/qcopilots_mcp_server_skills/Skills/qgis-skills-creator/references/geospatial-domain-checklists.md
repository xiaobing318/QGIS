# 地理数据域检查清单

只读取并应用与当前任务 `geospatial_semantics.domains[]` 对应的章节。将选中域的约束写入计划、目标技能的 `references/data-contract.md` 和内容级验收；对未选域只记录不适用理由，不复制整段模板。

## 目录

1. 跨域基线
2. 矢量与空间表
3. 栅格
4. 点云
5. 网格与时变模拟结果
6. 三维场景与瓦片
7. 地理服务与远程数据源
8. 时空数据
9. 验收证据

## 1. 跨域基线

所有域都先明确：

- 数据集、子图层、band、collection、dataset group 或 scene node 的稳定身份。
- 水平 CRS、轴顺序、坐标纪元；涉及高程时再明确垂直 CRS、基准面和单位。
- 空间范围、精度、容差、比例尺或分辨率，以及这些值使用的单位。
- 时间语义，包括时区、历法、时间单位、采样间隔和区间端点是否包含。
- provider/driver、格式版本、编码、压缩、sidecar、多层容器和索引要求。
- 输入只读、输出 `fail-if-exists`、临时数据生命周期和物理别名冲突检查。
- 数据规模、内存/磁盘预算、分块或流式策略、超时、取消和可恢复点。
- 内容级成功阈值、失败阈值和可复核证据；文件存在不等于处理成功。

不要猜测 CRS、垂直基准、时间单位或轴顺序。不要用赋予 CRS 代替坐标转换，也不要把水平 CRS 转换当作垂直基准转换。

## 2. 矢量与空间表

适用于 `vector`；无几何空间表可同时选择 `tabular`。

- 验证 geometry family、multipart、Z/M、空几何、无效几何和集合类型。
- 明确字段名、类型、宽度/精度、null、唯一性、domain、编码和主键策略。
- 明确拓扑规则，例如重叠、缝隙、自相交、重复点、悬挂线或多边形方向。
- 距离、面积、缓冲和简化先选择适合的投影与测量模型，不在经纬度上默认使用平面单位。
- 多层容器分别处理 dataset collision 与 layer collision；Shapefile 等多文件格式包含全部 sidecar。
- 验收至少覆盖可打开性、CRS、geometry、schema、feature count、extent、有效性和适用的拓扑规则。

## 3. 栅格

适用于 `raster`。

- 验证 band 数量、顺序、语义、data type、scale、offset、color table、mask 和 NoData。
- 明确 extent、width、height、pixel size、pixel origin、grid alignment 和像元解释。
- 区分连续、分类和布尔数据；分别选择 bilinear/cubic、nearest 或明确的聚合方法。
- 明确重投影、重采样、裁剪和 mosaic 的顺序，避免重复插值或意外扩大范围。
- 处理多分辨率、overview、压缩、tiling、BigTIFF/COG 与遥感元数据时写出格式约束。
- 验收至少覆盖可打开性、CRS、grid、bands、NoData、statistics/value range 和抽样像元。

## 4. 点云

适用于 `point-cloud`。

- 明确 LAS/LAZ/COPC/EPT 等格式和版本、point format、压缩、空间索引及流式访问能力。
- 验证 point count、bounds、水平/垂直 CRS、scale/offset 和坐标量化精度。
- 列出所有必需维度及其类型，例如 X/Y/Z、Intensity、ReturnNumber、Classification、GpsTime、RGB/NIR。
- 明确 classification code、return 语义、invalid/withheld/overlap 标志和缺失维度处理。
- 对 filter、thin、tile、classify、rasterize 或 merge 写出密度、邻域、边界和顺序效应。
- 大数据默认分块或流式执行，验证输出点数、bounds、维度、分类分布、密度和抽样坐标误差。

## 5. 网格与时变模拟结果

适用于 `mesh`。

- 明确顶点、边、面或体单元类型，拓扑连通性、无效/退化单元和边界条件。
- 明确 dataset group、scalar/vector、位置语义（vertex/face/volume）、垂直层和单位。
- 对时变组记录时间原点、单位、step、可用范围和 interpolation/nearest 规则。
- 明确重投影或采样是否只改变坐标、是否需要重建拓扑，以及矢量分量如何旋转。
- 导出栅格/矢量时写出采样位置、分辨率、mask、NoData 和时间选择。
- 验收覆盖拓扑、CRS、dataset groups、时间步、值域、缺失值和代表性单元抽样。

## 6. 三维场景与瓦片

适用于 `three-dimensional`。

- 同时明确水平与垂直参考、坐标轴方向、up axis、单位、局部原点和大坐标偏移策略。
- 明确 geometry/feature identity、Z 来源、extrusion、terrain/ellipsoid/orthometric height 语义。
- 对 3D Tiles、I3S、glTF 或场景包记录格式版本、tileset hierarchy、bounding volumes、geometric error 和 LOD。
- 验证材质、纹理 URI、颜色空间、法线、透明度、外部资源和嵌入资源策略。
- 明确相机或 QGIS 3D 视图状态是否属于输出，避免把仅显示变化误报为数据变化。
- 验收覆盖 bounds、参考系、轴/单位、LOD 完整性、资源可达性、抽样高度和可渲染性。

## 7. 地理服务与远程数据源

适用于 `geospatial-service`。

- 明确协议与版本，例如 WMS/WFS/WCS/WMTS、XYZ、OGC API、STAC、数据库或对象存储接口。
- 绑定 exact endpoint、layer/collection/tileset ID、operation、media type、schema 和服务 CRS。
- 对 WMS 1.3 等轴顺序差异、tile matrix set、scale denominator、origin 和 row/column 规则显式验证。
- 明确 pagination、limit、filter dialect、server-side CRS/format negotiation 和一致性快照。
- 凭据只引用宿主安全入口，不写入计划、技能、日志或 URI；记录 TLS、代理和证书要求。
- 记录 rate limit、retry/backoff、timeout、cache/ETag、离线行为和部分响应处理。
- 网络策略只允许计划中的 exact endpoint；重定向、跨主机资源和签名 URL 必须重新评估。
- 验收覆盖状态码、content type、schema、分页完整性、空间/时间范围、CRS、服务错误和限流行为。

## 8. 时空数据

适用于 `spatiotemporal`，并与实际数据域组合使用。

- 明确 instant/interval、时区、历法、时间原点、单位、排序、重复时间和缺测语义。
- 明确空间与时间 filter 的执行顺序、边界包含关系、窗口、aggregation 和 interpolation。
- 对轨迹验证 entity identity、点序、速度/加速度单位、跳点和跨日期变更线处理。
- 对动态栅格、网格或服务验证每个时间步的 grid/topology/schema 是否稳定。
- 验收覆盖时间范围、step 数、顺序、缺测、边界样本和代表性时刻的空间内容。

## 9. 验收证据

每个选中域至少准备一个正常、一个边界和一个失败用例。证据应包含：

- 输入身份、适用元数据与去除秘密后的参数。
- 实际工具、provider、exact algorithm/operation 和版本。
- 输出内容统计、抽样记录或像元/点/单元，以及阈值判断。
- CRS、垂直基准、单位、时间、范围和数据域专属约束的逐项结果。
- warning、error、取消、部分输出、collision、幂等和清理结果。

只有与 `domains[]` 对应的必需检查全部有实际结果时才声明成功。
