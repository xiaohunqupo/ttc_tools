# 变更日志

## 0.3.1

拿真实字体把公开 API 跑了一遍：7 个真集合（21 个面）全项通过，另外 8 个非 TTC 文件（透明加密
包装）被一致地拒绝；期间发现并修掉两个库缺陷，以及测试对语料的一个错误假设。

### 修复

- **`ensure_directory()` 把"路径被一个文件占用"报成成功。** `mkdir` 对"已存在的目录"和"同名
  的文件"都返回 `EEXIST`，旧实现直接当作成功。现在补一次类型查询：路径被文件占用时返回
  `false` + `err_mkdir_failed`，消息写明 `a file with that name is in the way`。落盘本身此前
  也会因此失败，所以这不是数据错误，而是**诊断错误**——调用方拿到的是一次假成功。
- **`file_stem_for()` 的重名循环有魔法上限 10000**，超过后会静默返回一个重复名，让后一个面覆盖
  前一个。改为按 `used.size() + 1` 个互不相同的候选去试：候选数严格多于已用名数，必然存在空闲
  名，也不再依赖任意常数。

### 测试

- **测试不再假设语料都是 TTC。** 原先"真实集合"一节对每个语料直接断言 `ref::parse_ttc()` 成功，
  于是 `fonts/` 里一旦出现非 TTC 文件（加密包装、残缺文件）测试就红。现在先让库与测试自带的
  独立读取器各自判断，**两者结论必须一致**：都接受 → 全套逐表校验；都拒绝 → 逐个入口断言拒绝
  行为（返回值、错误码、输出容器为空）；只有结论不一致才算失败。语料目录因此可以放心放"坏文件"。
- 新增用例：路径被同名文件占用时的 `ensure_directory()`；64 个同名字面连续取文件名必须两两
  不同；父目录不存在时 `write_file()` 的错误码；`parent_directory()` / `join_path()` 的边界。
- 测试用例名与清理逻辑对残留状态更健壮（上一轮中断留下的同名目录不会让下一轮误判）。

### 验证

- 7 个真实集合（cambria、mingliub、msgothic、msjh、msjhbd、msjhl、Nirmala，共 21 个面）：
  每个面逐表与源集合比对、每张表校验和、整文件 `0xB1B0AFBA`、4 字节对齐、`head.magicNumber`
  全部通过；产出的 21 个 `.ttf` 都能被 ttf2mesh 1.6 加载。
- 8 个非 TTC 文件（随机字节 + `PA31` 标记的加密包装）：五个入口全部 `err_not_a_collection`，
  无输出、无残留文件。
- 独立 API 检查程序（不在仓库内）5948 项断言：全部公开接口、Unicode 文件名（CJK family 落盘后
  可读回）、磁盘与内存结果逐字节一致、覆盖写；`Sfnt` / `Collection` 与自由函数结果互相印证。
- 模糊测试：对有效集合 400 轮变异（截断/翻转字节/改头），对每个非 TTC 文件各 40 轮，合计
  1000+ 轮无崩溃；拒绝时错误码与消息齐备，被接受的变异产物仍然通过结构校验。

### 新增

- **`Sfnt::verify()` + `Sfnt_Report`**：校验一个独立字体（`build()` 的产物，或任意 `.ttf`/`.otf`）
  —— 逐表重算校验和、4 字节对齐、`head.magicNumber`、整文件 `0xB1B0AFBA`。只额外拷 `head`
  （54 字节），可以在 debug 构建或自检里直接断言。`Sfnt_Report::first_problem` 给出可直接打印
  的第一处问题。
- **`with_scratch_copy(bytes, consumer)`**：把一份**一次性副本**交给"声明 `const` 却会写进去"的
  消费方，调用方的字节原样保留。这正是 `ttf2mesh` 需要的那层隔离：

  ```cpp
  const int rc = ttc_tools::with_scratch_copy(face, [&](const std::uint8_t* data, std::size_t size) {
      return ttf_load_from_mem(data, (int)size, &ttf, false);
  });
  // face 与调用前逐字节相同
  ```

### 文档

- README 增加"不是 TTC 的文件"一节（加密包装的识别特征与期望行为）。
- README 增加"下游会写穿 `const`"一节，把 `ttf2mesh` 的行为讲清楚：它**不是重算校验和**，而是把
  表目录与若干头结构**原地 big→host 转序**（实测 cambria 49 254 字节、mingliub 197 404 字节被改，
  表记录的 `checkSum`/`offset`/`length` 全部前后颠倒），结果是混合字节序、**不可修复**。给出两条
  正确用法（`with_scratch_copy()` 或重新 `UnzipTTCFace()`）与一条自检（`Sfnt::verify()`）。

## 0.3.0

一次以"能托付"为目标的收尾：修掉 0.2.0 里跑不通的接口、把逐字节 I/O 换成内存搬运、给外部
输入加上边界校验，按 `shx_tools` 的约定重排目录与构建，并补齐测试与文档。

### 架构与构建

- **移除 Visual Studio 工程**（`Project/*.vcxproj`），构建只走 CMake：`inc/` 公开头文件、
  `src/` 实现、`sample/` 示例、`test/` 测试，与 `shx_tools` 完全一致。
- **版本号由项目自己维护**：`TTC_TOOLS_VERSION_MAJOR` / `MINOR` / `PATCH` / `STRING` 写在
  `inc/ttc_tools/ttc_tools.h` 开头（与 `shx_tools` 的 `SHX_TOOLS_VER_*` 同一位置），升级就是
  改这一处加一条变更日志。CMake 不再配置版本，`project()` 也不声明 `VERSION`，避免两个数字
  打架；`config.h.in` 只保留构建方式开关 `TTC_TOOLS_HEAD_ONLY`。
- **`TTC_TOOLS_HEAD_ONLY` 开关**（默认 `ON`）：开启时头文件末尾直接 include 同名 `.cpp`，
  函数用 `TTC_TOOLS_HEAD_ONLY_INLINE` 保持 `inline`，库是 INTERFACE 目标；关闭时 `src/*.cpp`
  编成静态库。两种模式都在本机验证过（`ctest` 1/1 通过）。
- 单文件 `include/ttc_tools.ipp` 拆成六个模块：`ttc_error`（错误码）、`ttc_file`（整文件读写
  与目录）、`ttc_sfnt`（`Sfnt` 类：一个面的表目录 + 重建）、`ttc_name`（`name` 表与文件名
  规则）、`ttc_collection`（`Collection` 类与 `ListTTC`/`UnzipTTC*`）、`ttc_extract`（落盘）。
  内部辅助集中在 `src/ttc_tools/common.h`。
- **新增类接口** `Collection` / `Sfnt`：可以只解析集合头、只解析一个面的表目录，或单独调用
  `Sfnt::build()` / `Sfnt::checksum()` / `Sfnt::tag_string()`；0.3.0 的自由函数全部保留。
- 示例程序改成两个：`ttcsplit`（拆分/列面/单面导出）与 `ttcdump`（打印表目录并验证每张表
  校验和与整文件校验和，退出码是问题数）。
- 补 `.github/workflows/ci.yml`：Windows 与 Linux，`TTC_TOOLS_HEAD_ONLY` 取 `ON` / `OFF`
  各跑一遍 configure + build + ctest。

### 修复

- **内存接口原先什么都不返回。** `unzipTTC_Memery()` 把结果写进局部变量 `tempData`，从未写回
  输出参数，函数末尾也没有 `return`（MSVC `/W4` 报 C4715、C4100）。实测返回 `true` 而
  `ttfs.size() == 0`。现在是 `UnzipTTC()` 的别名，实测 2 个字面全部返回，另加拼写正确的
  `unzipTTC_Memory()`。
- **失败被当成功。** 输出目录不存在时旧实现什么都不写却返回 `true`；单个字面 `fopen` 失败也
  只是 `continue`。现在目录会自动创建（可多级），任一字面写入失败即整体失败并给出
  `err_write_failed`。
- **不校验输入。** 16 字节的伪造集合头（`numFonts = 0x7FFFFFFF`）会让 0.2.0 分配 8.6 GB 并
  空转；字面偏移越界会抛未捕获的 `std::length_error`；表数读到垃圾值时返回 `true` 并写出坏
  文件。现在有 10 项边界校验（见 README 表格），全部走错误码，不抛异常、不做超额分配。
- **表头 `searchRange` / `entrySelector` / `rangeShift` 改为重算**，不再照抄源文件。

### 性能

- 读路径从"逐字节 `getc` + 每表 `fseek`"改为"整文件一次 `fread` + `memcpy` 区间 + 一次
  `fwrite`"，输出缓冲一次分配。
  - `simsun.ttc`（18 MB / 2 面）：1 618 ms → **17 ms**（≈ 95×）
  - `mingliub.ttc`（37 MB / 4 面）：6 899 ms → **76 ms**（≈ 90×）
  - 15 个系统字体合计：≈ 25 s → ≈ 0.3 s

### 新增

- `ListTTC()`：只读集合头与 `name` / `maxp`，列出字面（家族名、子家族名、glyph 数、表数、
  sfnt 类型）而不复制数据。
- `UnzipTTC()` / `UnzipTTCFace()` 的内存版与单面版；单面版避免"面数 × 文件长度"的内存峰值。
- `UnzipTTCToDir()` 与 `UnzipTTCFaceToFile()`：落盘接口，返回写出的路径列表。
- `error_info` / `error_code` / `error_name()`：可打印的失败原因，`nullptr` 时可忽略。
- 以 `name` 表命名输出文件（`SimSun.ttf` 而不是 `face_00.ttf`），处理非法字符、UTF-8 截断、
  重名（`_<subfamily>`、`_2`）、无 `name` 表回退，并按 sfnt 版本选择 `.ttf` / `.otf`。
- 跨平台：去掉 `fopen_s` / `sprintf_s` / `_MAX_PATH`；Windows 上用 `_wfopen` + UTF-8→UTF-16
  转换，非 ASCII 字体名也能落盘；目录创建用 `_wmkdir` / `mkdir`。
- 示例程序改为真正的命令行（`-l` / `-x` / `-v` / `-h`），不再是硬编码路径的空壳。
- `test/test_ttc.cpp`（自包含回归测试）、`docs/ttc_format.md`（格式与算法说明）、
  `.gitignore`、`.github/workflows/ci.yml`。

### 测试

- 合成夹具覆盖：双字面往返、共享表池、2.0 头部、`OTTO` 面、奇数长度表的 4 字节填充、
  重名与无名字面命名、目录自动创建、兼容接口、12 类畸形输入，以及 `Sfnt` / `Collection` /
  文件名规则的公开接口。
- 断言独立于实现：测试自带一份 sfnt/ttc 读取器，逐表比对源集合与输出、校验每张表校验和、
  校验整文件校验和 `0xB1B0AFBA`、校验 4 字节对齐与 `head.magicNumber`。
- 真实集合由 CMake 把 `fonts/*.ttc` 通配后作为参数传入，没有字体时退回
  `TTC_TOOLS_TEST_FONT` 与系统字体路径。本机 15 个系统集合、36 个字面全部通过。

### 仓库

- 移除了随仓库提交的 18 MB `fonts/simsun.ttc`（中易宋体，厂商专有字体，MIT 只覆盖代码）。
  本地保留的文件不受影响，测试会自动使用它；仓库内改为 `fonts/README.md` 说明。

### 兼容性

- 0.2.0 的两个函数名与签名保持不变：`UnzipTTc(file, dir, prefix)`、`unzipTTC_Memery(file, ttc)`。
  两者的行为都变好了（前者会建目录并按字体名命名，后者真的返回数据），但不再"静默成功"。
- `ttc_tools::TTC` 增加 `version` 与 `faces` 两个成员，`ttfs` 语义不变。

## 0.2.0

- 初版：读取 `.ttc` 头，逐字面复制表数据、重写表偏移、补 `head.checkSumAdjustment`，
  支持落盘与"进内存"两个入口（后者未写回输出参数）。
