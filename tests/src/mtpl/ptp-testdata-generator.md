# PTP 回归语料生成器

`mtpl_testdata_generator` 随 QGIS 的 MTPL 测试目标编译，但不注册为 CTest 测试。只有显式运行它才会写入测试数据。生成器通过现有 `writePtpFixture` 调用 MTPL writer API，通过 `rasterTileImage` 使用 Qt 生成不依赖字体的 PNG、JPEG、WebP 瓦片。

在 QGIS RelWithDebInfo 运行环境中执行。下面假定当前目录是生成器所在的可执行文件目录。

```powershell
.\mtpl_testdata_generator.exe --output-root "C:\Data\QGISData\MTPLData\synthetic\ptp-regression-v2"
.\mtpl_testdata_generator.exe --verify --output-root "C:\Data\QGISData\MTPLData\synthetic\ptp-regression-v2"
```

生成要求目标目录尚不存在。目标路径已经存在时会立即失败，不覆盖原有数据，也不清理任何语料目录。中途失败会保留新建的部分输出用于检查。修正问题后可指定另一个新目录重试。

生成和验证都对清单中的全部文件校验大小与 SHA-256，也会拒绝遗漏和新增的未登记文件。随后实际调用包探测和数据集构建接口，检查每个 PTP、正例叶目录及缺少密钥、混合凭据、错误包名等负例。压力样例额外断言矩阵保持 9×9 且只包含 Z0，防止错误元数据被默认值掩盖。

工具页输入树和元数据用例会在独立临时目录中分别执行明文、加密 PTP 创建，正例继续回载验证，负例检查没有提交输出或留下临时文件。验证模式不修改现有语料，临时操作输出和临时 QGIS 用户配置在进程结束时清理，不使用用户的真实认证配置。实际窗口交互、源文件替换后刷新和项目保存重开仍需按操作说明执行。

原 `synthetic` 的 29 套场景保持不动。新目录独立提供：

- 工具页原始 `{z}/{x}/{y}.ext` 输入树，包括 PNG、JPEG、WebP、使用 `.bin` 后缀的真实 PNG、匹配 33/129/256 尺寸的正例、多层级，以及后续瓦片损坏、尺寸不匹配、重复坐标、越界坐标、Z31 和错误目录结构。截断负例包括只保留前三分之一的 JPEG 和末尾删去两字节的 PNG，避免把 Qt 容错解码成功误判为源文件完整
- 非法 JSON、非对象 JSON、高程类型声明、无效瓦片矩阵和尺寸冲突的元数据输入，以及分包输出文件名与输入层级冲突的操作步骤
- 空加密包、同一数据集混合明文和加密包、分别使用两组凭据的包
- 9×9 根矩阵的 81 个同屏 PTP 包，避免把稀疏但无法同屏命中的大量包误作句柄缓存压力测试
- 实际绝对路径超过 280 字符的 Unicode 路径、源文件替换辅助包和加密项目恢复辅助包

`credentials/a.json` 和 `credentials/b.json` 只包含测试代码中的固定合成凭据，与用户数据密钥无关。读取其中 `privateKeyBase64` 和 `deviceKeyHex` 字段，填入插件的对应输入框。不要用这些文件解锁 `gr` 或 `ModifiedData`。

`EXPECTED_RESULTS.md` 记录每项操作和期望。`MANIFEST.json` 的 `cases` 数组包含 `id`、`path`、`kind`、`expected`、`notes`，需要凭据的场景另有 `credential_id`。`files` 数组记录相对路径、大小和整文件 SHA-256。`packages` 记录 PTP 元数据摘要、是否加密、瓦片坐标、编码字节摘要和可解码瓦片的 RGBA 摘要。清单不包含密钥值。

RGBA 摘要以 Qt 解码后 `Format_RGBA8888` 的逐行有效像素字节计算，不包含行填充。PNG 方向标记按左上红、右上绿、左下蓝、右下黄排列。不同 `marker` 改变中心区域颜色，便于识别替换后是否仍显示旧缓存。JPEG/WebP 的摘要依赖该次生成使用的 Qt 编解码器，清单用于验证这一份语料，而不是承诺跨编解码器版本具有相同文件字节。

图层加载应选择直接含 PTP 的数据集叶目录。语料根、`raw`、`source-changes` 等父目录不是可合并的数据集。转换、覆盖替换和项目移动请对独立副本操作，再用原始语料清单验证基线未变化。生成器不创建历史格式的 QGIS 项目文件。旧格式迁移必须使用包含历史 `<mtpl-package>` 节点的已有工程，或专门构造旧 XML 的测试副本。当前版本即使加载单个文件，保存时也使用 dataset 格式，因此保存并重开这种工程只验证当前格式恢复，不等于验证旧格式迁移。图像内容以外的 PBF 数据不属于这份增补语料。
