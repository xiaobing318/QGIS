# Processing MCP Capabilities

- Generated at (UTC): `2026-03-12T08:10:40.636844Z`
- Exporter module: `I:\github_repos\QGIS\python\plugins\processingmcpserver\capabilities_markdown.py`
- Package directory: `I:\github_repos\QGIS\python\plugins\processingmcpserver`
- Tool count: `47`
- Prompt count: `1`
- Resource count: `3`

## Tools

### `common_get_qgis_info`

用途：返回当前 QGIS Desktop 会话、平台、插件与 Processing MCP 运行状态，适合作为所有自动化流程的环境探测入口。 输入语义：无业务输入。 前置条件：QGIS Desktop 已启动且 processingmcpserver 插件已加载。 主要副作用：无写操作，只读取当前应用、项目与插件状态。 安全开关：无。 返回结果：返回 qgis、platform、python、active_project、active_plugins，以及 processing_mcp.filesystem.write_policy 安全摘要等环境信息。

### `vector_add_layer`

用途：把单个矢量数据源加载到当前 QGIS 工程。 输入语义：path 指向矢量数据文件或数据源，provider 默认 ogr，name 可覆盖图层显示名。 前置条件：目标路径必须存在且能被对应 provider 正常识别。 主要副作用：会向当前工程新增一个矢量图层，但不会改写源数据文件。 安全开关：无 destructive 开关；若路径无效会直接报错并中止加载。 返回结果：返回新图层的 id、name、type 与 feature_count，便于后续继续引用。

### `vector_add_layers`

用途：批量把多个矢量数据源加载到当前 QGIS 工程。 输入语义：paths 是待加载路径数组，provider 默认 ogr，skip_invalid 控制遇到坏数据时是跳过还是整体失败。 前置条件：至少提供一个可访问路径，且对应数据源应能被 provider 识别。 主要副作用：会向当前工程新增多个矢量图层，但不会改写源数据文件。 安全开关：skip_invalid=true 时会尽量继续加载其余路径；skip_invalid=false 时任何一个失败都会报错终止。 返回结果：返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 的逐项结果。

### `raster_add_layer`

用途：把单个栅格数据源加载到当前 QGIS 工程。 输入语义：path 指向栅格文件或数据源，provider 默认 gdal，name 可覆盖图层显示名。 前置条件：目标路径必须存在且能被 GDAL 或指定 provider 识别。 主要副作用：会向当前工程新增一个栅格图层，但不会改写源数据文件。 安全开关：无 destructive 开关；路径或数据源无效时直接失败。 返回结果：返回新图层的 id、name、type、width、height 与 band_count。

### `raster_add_layers`

用途：批量把多个栅格数据源加载到当前 QGIS 工程。 输入语义：paths 是待加载路径数组，provider 默认 gdal，skip_invalid 控制遇到坏数据时是跳过还是整体失败。 前置条件：至少提供一个可访问路径，且对应数据源应能被 provider 识别。 主要副作用：会向当前工程新增多个栅格图层，但不会改写源数据文件。 安全开关：skip_invalid=true 时会跳过坏数据继续执行；skip_invalid=false 时任一失败都会中止。 返回结果：返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 的逐项信息。

### `layer_list`

用途：列出当前工程中可见或全部图层的基础清单，用于给模型建立当前项目上下文。 输入语义：layer_types 可选 vector、raster 或 both，include_hidden 控制是否包含图层面板中隐藏图层，name_glob 用于名称通配过滤。 前置条件：当前工程中至少存在已加载图层时结果更有意义，但空工程也可调用。 主要副作用：无写操作，只读取工程图层注册表与图层树可见性。 安全开关：无。 返回结果：返回按过滤条件匹配的图层数组，元素包含 id、name、type、visible、provider 以及矢量或栅格的补充摘要。

### `layer_get_panel_tree`

用途：读取图层面板树结构，帮助模型理解分组、顺序和隐藏状态。 输入语义：include_hidden 控制是否把隐藏图层与分组也放入返回结构。 前置条件：当前工程已初始化即可调用，最好已有图层树内容。 主要副作用：无写操作，只读取 QgsLayerTree 结构。 安全开关：无。 返回结果：返回 tree 主结构，以及为兼容旧调用保留的 groups、layers 扁平摘要，便于模型按图层面板顺序推理。

### `layer_get_details`

用途：按图层 id 或图层名读取单个图层的详细元数据。 输入语义：layer_ref 可以是唯一 layer id，也可以是能唯一解析的图层名称。 前置条件：目标图层必须已经加载到当前工程，名称引用不能歧义。 主要副作用：无写操作，只读取图层元数据与字段摘要。 安全开关：无。 返回结果：返回 id、name、type、provider、source、crs，以及矢量的 fields 和 feature_count 或栅格的尺寸与波段数。

### `layer_remove`

用途：从当前工程中移除单个图层。 输入语义：layer_id 必须是当前工程已存在的 layer id，不接受模糊名称。 前置条件：目标图层必须已经在当前工程注册。 主要副作用：会修改当前工程图层列表和图层面板，但不会删除底层数据文件。 安全开关：无 confirm_destructive 开关，因此调用前应先用 layer_list 或 layer_get_details 复核目标 id。 返回结果：返回 removed 字段，值为已移除的 layer id。

### `layer_remove_batch`

用途：从当前工程中批量移除多个图层。 输入语义：layer_ids 是 layer id 数组，空白值会被忽略。 前置条件：建议先用 layer_list 确认待删除 id；不存在的 id 不会抛错，而是记录到 missing。 主要副作用：会修改当前工程图层列表和图层面板，但不会删除底层数据文件。 安全开关：无 confirm_destructive 开关，因此应只传入已确认的 layer id。 返回结果：返回 removed 与 missing 两个数组，便于区分成功移除和未命中的目标。

### `layer_resolve_references`

用途：把图层名称或 layer id 解析成唯一 layer id，便于后续安全调用其它工具。 输入语义：refs 是待解析引用数组，strict 控制遇到 missing 或 ambiguous 时是返回详情还是直接失败。 前置条件：目标引用应来自当前工程；若名称重复会被归类为 ambiguous。 主要副作用：无写操作，只读取当前工程图层注册表。 安全开关：strict=false 时会尽量返回 resolved、missing、ambiguous；strict=true 时只要存在缺失或歧义就抛错。 返回结果：返回 resolved 映射、missing 数组和 ambiguous 映射。

### `vector_get_layer_features`

用途：提取矢量图层的样本要素，供模型观察字段值和几何概貌。 输入语义：layer_ref 指向矢量图层，limit 是希望返回的要素数。 前置条件：目标图层必须存在且为矢量图层。 主要副作用：无写操作，只顺序读取要素并序列化属性与 geometry_wkt。 安全开关：limit 会被内部最大阈值裁剪，返回里会明确给出 requested_limit、applied_limit 与 limit_capped。 返回结果：返回 layer_id、feature_count、fields、features 以及 limit 应用结果。

### `vector_table_add_field`

用途：为矢量图层增加一个新字段。 输入语义：layer_ref 指向矢量图层，field_name 是新字段名，field_type、field_length、field_precision 用于定义字段类型，in_place 控制是否直接改源图层。 前置条件：目标图层必须存在，且 field_name 不能为空且不能与现有字段重名。 主要副作用：会修改图层字段结构；默认副本模式下先复制图层再修改。 安全开关：默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回结果：返回 summary.mode、affected_count、output_layer_id 以及最新字段列表。

### `vector_table_drop_fields`

用途：删除矢量图层中的一个或多个字段。 输入语义：layer_ref 指向矢量图层，fields 是待删除字段名数组，in_place 控制是否直接改源图层。 前置条件：目标图层必须存在，且至少提供一个字段名。 主要副作用：会修改图层字段结构；不存在的字段不会阻止执行，而是记录到 missing_fields。 安全开关：默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回结果：返回 summary.mode、affected_count、output_layer_id、剩余字段列表和 missing_fields。

### `vector_table_rename_field`

用途：重命名矢量图层中的单个字段。 输入语义：layer_ref 指向矢量图层，field_name 是旧字段名，new_field_name 是新字段名，in_place 控制是否直接改源图层。 前置条件：目标图层必须存在，旧字段必须存在，新字段名不能与已有字段重名。 主要副作用：会修改图层字段结构；默认副本模式下会生成新的输出图层。 安全开关：默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回结果：返回 summary.mode、affected_count、output_layer_id 和重命名后的字段列表。

### `vector_table_calculate_field`

用途：按 QGIS 表达式批量计算字段值，可在字段不存在时自动创建字段。 输入语义：layer_ref 指向矢量图层，field_name 是目标字段，expression 是赋值表达式，where 可选筛选条件，field_type 只在自动建字段时生效，in_place 控制是否直接改源图层。 前置条件：目标图层必须存在，expression 和 where 若提供都必须能被 QGIS 表达式解析并成功求值。 主要副作用：会更新匹配要素的属性值；字段不存在时会先新增该字段。 安全开关：默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回结果：返回 summary.mode、affected_count、field_created、output_layer_id 和更新后的字段列表。

### `vector_table_query_records`

用途：按条件查询矢量图层记录，适合做数据抽样、分页和字段级检查。 输入语义：layer_ref 指向矢量图层，where 是可选过滤表达式，fields 控制返回字段，limit 和 offset 控制分页，order_by 指定简单排序字段，include_geometry 控制是否附带 geometry_wkt。 前置条件：目标图层必须存在，where 表达式和字段名必须有效。 主要副作用：无写操作，只读取并序列化匹配记录。 安全开关：limit 会被内部阈值裁剪；order_by 仅按属性字符串表示做简单排序，不是完整 SQL 排序。 返回结果：返回 matched_total、returned、offset、limit 应用结果和 records 数组。

### `vector_table_insert_records`

用途：向矢量图层批量插入新记录，可选携带 geometry_wkt。 输入语义：layer_ref 指向矢量图层，records 是对象数组，键名按字段名匹配，geometry_wkt 若提供会被解析为新要素几何，in_place 控制是否直接改源图层。 前置条件：目标图层必须存在，records 不能为空，geometry_wkt 若提供必须是有效 WKT。 主要副作用：会新增要素；未知字段不会写入，而是被忽略并记录到 warnings 与 outputs.ignored_fields。 安全开关：默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回结果：返回 summary.mode、affected_count、output_layer_id、feature_count，以及 ignored_fields 和 warnings。

### `vector_table_update_records`

用途：按条件批量更新矢量图层记录，支持常量赋值和表达式赋值。 输入语义：layer_ref 指向矢量图层，where 是可选筛选表达式，set_literals 传常量值映射，set_expressions 传表达式映射，in_place 控制是否直接改源图层。 前置条件：目标图层必须存在，至少提供 set_literals 或 set_expressions 之一，涉及字段和表达式都必须有效。 主要副作用：会更新匹配要素属性值；默认副本模式下生成新图层后再更新。 安全开关：默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回结果：返回 summary.mode、affected_count、output_layer_id 和更新后 feature_count。

### `vector_table_delete_records`

用途：按条件删除矢量图层中的部分记录。 输入语义：layer_ref 指向矢量图层，where 可选筛选表达式，in_place 控制是否直接改源图层，confirm_destructive 必须明确确认删除行为。 前置条件：目标图层必须存在；若提供 where，表达式必须有效。 主要副作用：会删除匹配记录；默认副本模式下删除的是副本图层中的记录。 安全开关：默认 in_place=false；删除类操作还要求 confirm_destructive=true，避免误删原图层或记录。 返回结果：返回 summary.mode、affected_count、output_layer_id 和删除后的 feature_count。

### `vector_table_truncate`

用途：清空矢量图层中的全部记录，但保留图层结构和字段。 输入语义：layer_ref 指向矢量图层，in_place 控制是否直接改源图层，confirm_destructive 必须明确确认清空行为。 前置条件：目标图层必须存在。 主要副作用：会删除全部要素；默认副本模式下清空的是副本图层。 安全开关：默认 in_place=false；删除类操作还要求 confirm_destructive=true，避免误删原图层或记录。 返回结果：返回 summary.mode、affected_count、output_layer_id 和清空后的 feature_count。

### `vector_stats_basic`

用途：计算矢量字段的基础统计量，适合先判断数值分布。 输入语义：layer_ref 指向矢量图层，field_name 是目标字段。 前置条件：目标图层必须存在且字段存在，字段最好是可统计的数值型字段。 主要副作用：无写操作，只读取字段值并做统计。 安全开关：无。 返回结果：返回 count、sum、mean、min、max、stdev 等基础统计摘要。

### `vector_stats_by_categories`

用途：按一个或多个分类字段汇总记录数或数值字段统计，适合做分组分析。 输入语义：layer_ref 指向矢量图层，category_fields 是分类字段数组，values_field 可选；为空时通常只做分组计数。 前置条件：目标图层必须存在，分类字段必须存在；若提供 values_field，该字段也必须存在。 主要副作用：无写操作，只读取要素属性并做分组汇总。 安全开关：无。 返回结果：返回按分类组合分组后的统计结果，便于模型理解类别分布。

### `raster_stats_basic`

用途：读取栅格单个波段的基础统计量。 输入语义：layer_ref 指向栅格图层，band 指定波段号，默认 1。 前置条件：目标图层必须存在且为栅格，指定波段必须有效。 主要副作用：无写操作，只读取栅格波段统计信息。 安全开关：无。 返回结果：返回目标波段的最小值、最大值、均值、标准差等基础统计摘要。

### `raster_stats_zonal`

用途：把栅格统计结果按分区写入矢量图层属性表，常用于区域统计。 输入语义：vector_layer_ref 指向分区矢量图层，raster_layer_ref 指向栅格图层，raster_band 指定波段，column_prefix 控制输出字段前缀，in_place 控制是否直接写回原矢量图层。 前置条件：矢量与栅格图层都必须存在，且空间关系应满足分区统计需求。 主要副作用：会运行原生 zonal statistics 算法，并在矢量属性表增加统计字段；默认副本模式下生成新的输出图层。 安全开关：默认 in_place=false，会先生成副本图层并返回新的 output_layer_id；仅在明确要修改原图层时才把 in_place 设为 true。 返回结果：返回 summary.mode、algorithm、output_layer_id 和底层 Processing 结果对象。

### `raster_stats_cell`

用途：对多个栅格做逐像元统计运算，例如逐像元均值、最小值或最大值。 输入语义：raster_layer_refs 是栅格引用数组，statistic 选择逐像元统计类型，ignore_nodata 控制是否忽略 NoData。 前置条件：至少提供一个有效栅格图层，且各栅格最好在分辨率、范围和 CRS 上可兼容处理。 主要副作用：会运行 Processing 栅格统计算法，通常产生新的输出栅格结果；是否自动加载取决于算法默认行为和 Processing 设置。 安全开关：无 destructive 开关，但会触发 Processing 执行。 返回结果：返回算法摘要和序列化后的 Processing 输出结果。

### `dataset_list_files`

用途：扫描目录并识别可加载的数据集文件，区分 vector 与 raster。 输入语义：directory 是根目录，recursive 控制是否递归，dataset_kind 控制筛选 vector、raster 或 both，geometry_type 与 name_glob 用于进一步过滤，limit 控制返回上限。 前置条件：目录必须存在且可访问。 主要副作用：无写操作，只遍历文件系统并按扩展名和几何类型推断数据集。 安全开关：limit 会被内部阈值裁剪，geometry_type 过滤仅对识别出的矢量数据集生效。 返回结果：返回 datasets 数组及 requested_limit、applied_limit、limit_capped 等扫描摘要。

### `dataset_load_from_directory`

用途：先扫描目录中的候选数据集，再把匹配的 vector 或 raster 图层批量加载到当前工程。 输入语义：参数和 dataset_list_files 基本一致，skip_invalid 控制遇到坏数据时是跳过还是整体失败。 前置条件：目录必须存在，且目录下至少有可识别的数据集文件才有意义。 主要副作用：会向当前工程新增多个图层，但不会改写源数据文件。 安全开关：skip_invalid=true 时失败项会进入 failed；skip_invalid=false 时第一个失败就终止。 返回结果：返回 requested_count、loaded_count、failed_count，以及 loaded 和 failed 明细。

### `dataset_inspect_shapefile_bundle`

用途：扫描目录或单个 `.shp` 文件，检查 shapefile bundle 及其 sidecar 文件是否完整，并返回结构化风险摘要。 输入语义：path 可以是目录或单个 `.shp` 文件，recursive 控制目录递归扫描，name_glob 过滤候选文件，limit 控制返回上限，task_name 可把结果写入最近一次运行摘要。 前置条件：path 必须存在；若传文件则必须是 `.shp`。 主要副作用：无写操作，只读取 shapefile bundle 成员信息和矢量元数据。 安全开关：limit 会被内部阈值裁剪；task_name 非空时会更新 qgis://workflow/shapefile/run-summary。 返回结果：返回 bundles 数组，元素包含 `.shp/.shx/.dbf/.prj/.cpg` 完整性、大小、CRS、几何类型、字段名、命名风险和 warnings。

### `vector_check_validity_report`

用途：对单个矢量图层或矢量文件做 shapefile 导向的结构化体检，统一输出几何、记录、字段和 CRS 风险。 输入语义：layer_ref 与 path 二选一；required_fields 用于声明必需字段，expected_crs 用于声明目标 CRS，task_name 可把结果写入运行摘要。 前置条件：目标图层必须存在，或 path 必须指向有效矢量数据文件。 主要副作用：无写操作，只读取图层要素、字段和 CRS 元数据。 安全开关：task_name 非空时会更新 qgis://workflow/shapefile/run-summary；若传 expected_crs，结果会包含 CRS mismatch 判定。 返回结果：返回 report 对象，覆盖 null/empty geometry、invalid geometry、duplicate geometry、duplicate record、multipart、字段长度和 safe_for_export 判断。

### `vector_prepare_work_layer`

用途：把输入矢量图层整理成带任务标签的临时工作层，并串行执行 shapefile 稳定模板的标准化前置动作。 输入语义：layer_ref 与 path 二选一；task_name 用于标记工作层和运行摘要；target_crs 控制目标坐标系；normalize_field_names 控制是否把字段名压到 10 字符以内；multipart_policy 可选 keep 或 singleparts。 前置条件：目标图层必须存在或 path 指向有效矢量数据文件；target_crs 若提供必须可被 QGIS 解析。 主要副作用：会在当前工程新增一个临时工作层，并对该临时层执行空几何清理、几何修复、重复几何清理、重投影和可选字段改名/拆多部件操作。 安全开关：默认不写盘；所有修改都落在临时工作层上，原始输入图层和源文件不会被直接改写。 返回结果：返回 output_layer_id、field_name_mapping、initial_report、final_report，以及各标准化步骤的影响计数。

### `vector_export_shapefile`

用途：把当前工作层或任意矢量图层安全导出为最终 shapefile，并在导出前强制复核几何与字段约束。 输入语义：layer_ref 指向待导出的矢量图层，output_directory 是输出目录，file_name 可覆盖输出文件名，task_name 用于运行摘要，overwrite 控制是否允许覆盖，auto_truncate_field_names 控制是否自动裁剪超长字段名，confirm_write 与 confirm_destructive 用于显式确认写盘和覆盖。 前置条件：目标图层必须存在；output_directory 必须可创建；当 overwrite=true 时必须同时传 confirm_destructive=true。 主要副作用：会在磁盘写出 shapefile bundle；若需要自动裁剪字段名，内部会生成临时导出副本并在导出后自动移除。 安全开关：必须显式设置 confirm_write=true；覆盖现有输出时还必须 confirm_destructive=true；存在无效几何或空几何时会阻断导出。 返回结果：返回 output_path、output_members、field_name_mapping、preflight_report 和 final_report，便于在导出后继续走质量 gate。

### `project_cleanup_work_layers`

用途：按 task_name 或 layer_ids 清理 shapefile 工作流创建的临时图层，并可选删除显式登记的临时文件。 输入语义：task_name 用于按任务标签批量匹配工作层，layer_ids 可精确指定图层，temp_paths 可补充显式临时路径，delete_temp_files 控制是否删除这些路径，confirm_write 与 confirm_destructive 仅在删文件时生效。 前置条件：至少提供可匹配的 task_name、layer_ids 或 temp_paths 中的一类才有意义；若 delete_temp_files=true 且存在路径，则必须 confirm_write=true 且 confirm_destructive=true。 主要副作用：会从当前工程移除匹配的临时图层；可选地删除临时文件或临时目录。 安全开关：删除磁盘临时文件时需要显式双确认；仅移除工程图层时不触碰源数据文件。 返回结果：返回 removed_layer_ids、deleted_temp_paths 和 missing_temp_paths，便于回收检查与日志留存。

### `filesystem_query_list_entries`

用途：列出目录中的文件或子目录条目，适合在执行文件操作前先做只读探查。 输入语义：directory 是根目录，recursive 控制是否递归，include_files 和 include_directories 控制返回对象类型，name_glob 过滤名称，limit 控制返回上限。 前置条件：目录必须存在；include_files 与 include_directories 不能同时为 false。 主要副作用：无写操作，只读取文件系统元数据。 安全开关：limit 会被内部阈值裁剪；结果里会明确 returned_count、matched_total 与 truncated。 返回结果：返回目录路径、entries 数组以及 limit 应用摘要。

### `filesystem_query_entry_info`

用途：读取单个文件或目录的基础元数据。 输入语义：path 指向文件或目录。 前置条件：目标路径必须存在。 主要副作用：无写操作，只读取文件系统元数据。 安全开关：无。 返回结果：返回 entry 对象，包含类型、大小、时间戳和可用路径信息。

### `filesystem_query_read_text`

用途：按 UTF-8 读取文本文件内容，适合让模型读取配置、脚本或日志片段。 输入语义：path 指向文本文件，max_chars 可选，用于限制返回字符数。 前置条件：目标路径必须存在且是文件，并且内容应能按 UTF-8 解码。 主要副作用：无写操作，只读取文件内容。 安全开关：max_chars 为 None 时返回全文；传入数值时只读取 max_chars+1 个字符用于判断截断，并在 summary.truncated 中标记。 返回结果：返回 text 字段和截断摘要。

### `filesystem_edit_write_text`

用途：以 UTF-8 一次性写入文本文件，适合新建配置或覆盖写文件。 输入语义：path 是目标文件路径，content 是完整文本内容，overwrite 控制是否允许覆盖已存在文件，confirm_destructive 用于确认覆盖，create_parents 控制是否自动创建父目录，confirm_write 用于显式确认写操作。 前置条件：若目标已存在且 overwrite=false 会直接失败；父目录不存在时只有 create_parents=true 才会自动创建。 主要副作用：会创建或覆盖磁盘文件。 安全开关：所有 filesystem_edit_* 调用都要求 confirm_write=true；删除或覆盖时还必须显式设置 confirm_destructive=true。 返回结果：返回写入字符数和最终 path 摘要。

### `filesystem_edit_append_text`

用途：向文本文件尾部追加 UTF-8 内容。 输入语义：path 是目标文件路径，content 是待追加文本，create_parents 控制父目录不存在时是否自动创建，confirm_write 用于显式确认写操作。 前置条件：目标路径的父目录必须存在或允许自动创建；目标文件不存在时会被创建。 主要副作用：会在磁盘上创建文件或修改现有文件末尾内容。 安全开关：该工具不会覆盖已有内容，但仍属于写盘操作，调用前应确认目标路径。 返回结果：返回追加字符数和最终 path 摘要。

### `filesystem_edit_copy_entry`

用途：复制单个文件或整个目录到新位置。 输入语义：source_path 是源路径，target_path 是目标路径，overwrite 控制是否允许覆盖目标，confirm_destructive 用于确认覆盖已存在目标，confirm_write 用于显式确认写操作。 前置条件：源路径必须存在；目标若已存在且 overwrite=false 会失败。 主要副作用：会在磁盘上创建新的文件或目录副本；目录复制会递归复制内容。 安全开关：所有 filesystem_edit_* 调用都要求 confirm_write=true；删除或覆盖时还必须显式设置 confirm_destructive=true。 返回结果：返回 source_path 和 target_path 摘要。

### `filesystem_edit_move_entry`

用途：把文件或目录移动到新位置。 输入语义：source_path 是源路径，target_path 是目标路径，overwrite 控制是否允许覆盖目标，confirm_destructive 用于确认覆盖已存在目标，confirm_write 用于显式确认写操作。 前置条件：源路径必须存在；目标若已存在且 overwrite=false 会失败。 主要副作用：会修改磁盘目录结构，源路径在成功后会消失。 安全开关：所有 filesystem_edit_* 调用都要求 confirm_write=true；删除或覆盖时还必须显式设置 confirm_destructive=true。 返回结果：返回 source_path 和 target_path 摘要，表示移动已完成。

### `filesystem_edit_delete_entry`

用途：删除单个文件或整个目录树。 输入语义：path 指向待删除文件或目录，confirm_destructive 必须明确确认删除，confirm_write 用于显式确认写操作。 前置条件：目标路径必须存在。 主要副作用：会永久删除磁盘上的文件或目录内容。 安全开关：只有 confirm_write=true 且 confirm_destructive=true 才允许执行删除。 返回结果：返回 deleted_path 摘要。

### `filesystem_stats_directory`

用途：统计目录中的文件数、目录数和累计大小，适合在批处理前估算工作量。 输入语义：directory 是根目录，recursive 控制是否递归统计。 前置条件：目录必须存在。 主要副作用：无写操作，只遍历文件系统做统计。 安全开关：无。 返回结果：返回文件数、目录数、总字节数等目录统计摘要。

### `processing_list_providers`

用途：列出当前 QGIS Processing 注册表中的 provider 清单。 输入语义：无业务输入。 前置条件：Processing 运行时必须可用。 主要副作用：无写操作，只读取 Processing 注册表。 安全开关：无。 返回结果：返回 provider 的 id、name、active、algorithm_count，以及 count_scope、total_algorithm_count、active_provider_count、active_algorithm_count 和总数。

### `processing_get_algorithms`

用途：查询 Processing 算法目录，支持按 provider 过滤，或按 algorithm_id 精确读取单个算法。 输入语义：algorithm_id 提供时返回单个算法完整定义；provider_id 提供时按 provider 过滤；include_parameters 和 include_outputs 控制列表模式下是否展开参数和输出；limit 控制列表返回上限。 前置条件：Processing 运行时必须可用；若指定 algorithm_id，则该算法必须存在。 主要副作用：无写操作，只读取 Processing 算法元数据。 安全开关：列表模式的 limit 会被内部阈值裁剪；单算法模式会始终返回该算法的完整参数和输出定义。 返回结果：返回单个 algorithm 对象或 algorithms 数组，以及 count、returned、truncated 等摘要。

### `processing_get_parameter_template`

用途：把某个 Processing 算法的输入参数和输出定义整理成适合模型填写的模板。 输入语义：algorithm_id 是目标算法 id。 前置条件：Processing 运行时必须可用，且算法必须存在。 主要副作用：无写操作，只读取算法参数定义和输出定义。 安全开关：无。 返回结果：返回 algorithm 基本信息、required_parameters、optional_parameters 和 outputs。

### `processing_execute_algorithm`

用途：执行单次 Processing 算法调用，是通用算法执行入口。 输入语义：algorithm 是算法 id，parameters 是参数对象，load_results 控制是否把结果加载到当前工程，allow_disk_write 控制是否允许磁盘输出路径通过安全检查，allow_in_place_edit 控制是否允许原位编辑参数通过安全检查。 前置条件：Processing 运行时必须可用，algorithm 必须存在，parameters 必须是对象。 主要副作用：会触发 Processing 执行，可能生成临时输出、加载新图层，或在显式允许时写盘或原位修改。 安全开关：默认禁止磁盘写出和原位编辑；只有在明确需要时才把 allow_disk_write 或 allow_in_place_edit 设为 true，并应复核返回里的 safety_policy、warnings 与 effective_parameters。 返回结果：返回 algorithm、load_results、result、warnings、safety_policy 和 effective_parameters。

### `processing_execute_on_layers`

用途：把一组图层引用先解析成真实 layer id，再执行单次或批量 Processing 算法调用，适合模型按图层绑定自动批处理。 输入语义：algorithm 是算法 id，layer_bindings 把参数名映射到图层引用或引用数组，parameters 是其余参数对象，load_results 控制是否加载结果，batch_mode 控制是否按绑定数组逐批运行，allow_disk_write 与 allow_in_place_edit 控制安全检查。 前置条件：Processing 运行时必须可用，parameters 必须是对象，layer_bindings 必须能解析到有效图层；batch_mode=true 时每个绑定在对应索引都要有值。 主要副作用：会触发一次或多次 Processing 执行，并在每轮执行前把图层引用替换为真实 layer id。 安全开关：默认禁止磁盘写出和原位编辑；只有在明确需要时才把 allow_disk_write 或 allow_in_place_edit 设为 true，并应复核返回里的 safety_policy、warnings 与 effective_parameters。 batch_mode=false 时任一失败会直接抛错；batch_mode=true 时失败会记录到 runs 中继续处理后续批次。 返回结果：返回 ok、run_count、success_count、failure_count、runs、warnings、safety_policy 和最后一次 effective_parameters。

## Prompts

### `qgis_shapefile_pipeline_planner`

用途：生成面向 shapefile 的六阶段执行计划提示，约束模型按固定阶段完成路径检查、筛选、预检、统计、标准化、处理、导出和清理。 输入语义：task_name、input_dir、output_dir 为必填参数；quality_rule_resource 指向质量规则资源；deliverables 描述交付物说明。 前置条件：processingmcpserver 已注册 prompts/resources/tools，且建议同时挂载 qgis://workflow/shapefile/template 与 qgis://workflow/shapefile/quality-profile/default。 主要副作用：无写操作，只返回结构化 prompt 文本。 安全开关：无。 返回结果：返回六阶段中文主导提示模板，供 llama-server WebUI 的 Use Prompt 直接注入聊天上下文。

## Resources

### `qgis://workflow/shapefile/template`

用途：返回 shapefile 稳定模板的机器可读流程定义，供模型在 llama-server WebUI 中按固定阶段调用 tools。 输入语义：无业务输入；由固定 URI qgis://workflow/shapefile/template 标识。 前置条件：processingmcpserver 已注册 resources 能力，且 tools 暴露了 shapefile workflow template helper。 主要副作用：无写操作，只读取模板快照并封装为 resource envelope。 安全开关：无。 返回结果：返回包含 workflow_stages、required_inputs、defaults、stage_tool_map 与 phases 的 JSON 文本。

### `qgis://workflow/shapefile/quality-profile/default`

用途：返回 shapefile 默认质量规则，供模型在预检、标准化和导出阶段复用。 输入语义：无业务输入；由固定 URI qgis://workflow/shapefile/quality-profile/default 标识。 前置条件：processingmcpserver 已注册 resources 能力，且 tools 暴露了 shapefile quality profile helper。 主要副作用：无写操作，只读取默认质量规则并封装为 resource envelope。 安全开关：无。 返回结果：返回包含 quality_checks、blocking_rules、warning_rules、export_constraints 和 confirmation_policy 的 JSON 文本。

### `qgis://workflow/shapefile/run-summary`

用途：返回最近一次 shapefile 工作流运行的摘要 manifest，便于模型续跑、审计和导出后清理。 输入语义：无业务输入；由固定 URI qgis://workflow/shapefile/run-summary 标识。 前置条件：processingmcpserver 已注册 resources 能力，且当前会话中已初始化过 shapefile run summary。 主要副作用：无写操作，只读取最近一次运行摘要并封装为 resource envelope。 安全开关：无。 返回结果：返回包含 task_name、status、inputs、steps、warnings、outputs 和 generated_at 的 JSON 文本。
