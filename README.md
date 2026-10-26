# ttc_tools

把 TrueType Collection（`.ttc`）拆成独立的 `.ttf` / `.otf`。C++11，仅头文件，无第三方依赖。

版本 0.3.0 ｜ [变更日志](CHANGELOG.md) ｜ [格式说明](docs/ttc_format.md)

## 这个库解决什么问题

`.ttc` 是把多个字面（face）打包进一个文件，并让它们共用表数据的容器。Windows 上的中文字体
几乎都是这个格式：`simsun.ttc`、`msyh.ttc`、`msjh.ttc`、`msgothic.ttc`、`mingliub.ttc` …

而多数字体消费方只认单字体。最典型的是 [ttf2mesh](https://github.com/fetisov/ttf2mesh)
（TrueType 轮廓三角化）：把 `simsun.ttc` 直接交给 `ttf_load_from_file()` 会得到
`TTF_ERR_FMT`，因为它按 sfnt 头解析，`.ttc` 的 `ttcf` 头不在它的支持范围里。

`ttc_tools` 就是这一步适配：读集合 → 逐字面重建独立 sfnt（重写表目录、重算表校验和、补
`head.checkSumAdjustment`）→ 交回内存或写到磁盘。配合 `shx_tools`（`.shx` → 轮廓/字体）
与 `ttf2mesh`（`.ttf` → 2D/3D mesh），构成"CAD 图纸文字 → 几何/网格"的自有链路：

```
.shx  --shx_tools-->  轮廓 / .ttf / .otf ─┐
                                          ├─ ttf2mesh ─> 2D / 3D mesh
.ttc  --ttc_tools-->  .ttf / .otf ────────┘
```

## 快速开始

```cpp
#include "ttc_tools.h"

ttc_tools::error_info error;

// 1. 只看有哪些字面
std::vector<ttc_tools::Face> faces;
if (ttc_tools::ListTTC("C:/Windows/Fonts/simsun.ttc", faces, &error))
{
    for (std::size_t i = 0; i < faces.size(); ++i)
        std::printf("[%zu] %s %s, %u glyphs\n", i,
            faces[i].family.c_str(), faces[i].subfamily.c_str(), faces[i].num_glyphs);
}

// 2. 全部拆到目录（目录不存在会自动创建，文件按字体家族命名）
std::vector<std::string> written;
ttc_tools::UnzipTTCToDir("C:/Windows/Fonts/simsun.ttc", "out", "Face", &written, &error);
// -> out/SimSun.ttf, out/NSimSun.ttf

// 3. 只拆一个面进内存，直接喂给 ttf2mesh
ttc_tools::TTC::TTF face;
if (ttc_tools::UnzipTTCFace("C:/Windows/Fonts/simsun.ttc", 0, face, nullptr, &error))
    ttf_load_from_mem(face.data(), (int)face.size(), &ttf, false);
```

命令行（示例程序，也是最小用法参考）：

```
ttc_tools <input.ttc> [output_dir]       # 全部拆出
ttc_tools -l <input.ttc>                 # 列字面
ttc_tools -x <input.ttc> <index> <out>   # 拆单个面
ttc_tools -v | -h
```

集成方式三选一：

| 方式 | 做法 |
|---|---|
| CMake 子项目 | `add_subdirectory(ttc_tools)` 后 `target_link_libraries(app PRIVATE ttc_tools::ttc_tools)` |
| CMake 安装 | 安装后由 `ttc_toolsTargets.cmake` 提供 `ttc_tools::ttc_tools`（INTERFACE 目标） |
| 直接拷贝 | 只需 `include/ttc_tools.h` 与 `include/ttc_tools.ipp` 两个文件 |

## API

| 函数 | 作用 |
|---|---|
| `ListTTC(file/data, faces, error)` | 只读表目录与 `name`/`maxp`，不复制数据 |
| `UnzipTTC(file/data, ttc, error)` | 全部字面重建进内存，`ttc.ttfs[i]` 是完整 sfnt，`ttc.faces[i]` 是元数据 |
| `UnzipTTCFace(file/data, index, ttf, face, error)` | 只重建一个字面（大集合时内存友好） |
| `UnzipTTCToDir(file/data, dir, prefix, written, error)` | 全部落盘，返回写出的路径 |
| `UnzipTTCFaceToFile(file, index, path, face, error)` | 单面落到指定路径 |
| `UnzipTTc()` / `unzipTTC_Memery()` | 0.2.0 的旧名字，保留可用（后者在 0.2.0 里是有缺陷的，现在只是 `UnzipTTC()` 的别名） |

失败时返回 `false`，并把原因写进可选的 `error_info`（`code` 见 `error_code`，`message` 是
可直接打印的说明）。`error_name(code)` 给出短名，便于日志。

## 输出文件命名

1. 用 `name` 表的 family（id 1），非法字符替换为 `_`，去掉首尾空格与点，最长 120 字节且不截断
   UTF-8 序列；
2. 字面没有可用 `name` 表时用 `<prefix>_<序号>`；
3. 重名时追加 `_<subfamily>`，仍重名追加 `_2`、`_3` …；比较不区分大小写（Windows 语义）；
4. 扩展名按 sfnt 版本决定：`OTTO`（CFF 轮廓）写 `.otf`，其余写 `.ttf`。

## 校验与容错

输入被视为不可信数据（图纸带字体、客户机拷字体都是常见来源），所以解析路径上每一步都做边界
检查，任何异常都只返回错误码，不抛异常、不做超额分配：

| 检查 | 不通过时 |
|---|---|
| 文件 ≥ 12 字节 | `err_truncated` |
| 签名 == `ttcf` | `err_not_a_collection` |
| 版本 ∈ {1.0, 2.0} | `err_unsupported_version` |
| 字面数 > 0 | `err_empty_collection` |
| 字面数 ≤ 1024 **且** ≤ `(size-12)/4` | `err_bad_face_count` / `err_truncated` |
| 每个字面偏移 + 12 ≤ 文件长度 | `err_truncated` |
| 字面 sfnt 版本 ∈ {`0x00010000`, `true`, `OTTO`} | `err_bad_face` |
| 表数 1…4096 | `err_bad_table_count` |
| 表目录 12 + 16×表数 ≤ 文件长度 | `err_truncated` |
| 每张表 `offset + length` ≤ 文件长度 | `err_truncated` |

输出侧同样自检，测试里逐项验证：每个面 4 字节对齐、每张表自身校验和与目录一致、
`head.magicNumber` 保留、**整文件校验和 == `0xB1B0AFBA`**、表数据与源集合逐字节相同。

## 性能

0.3.0 改为"整文件一次读入 + `memcpy` 区间 + 一次写出"，替代 0.2.0 的逐字节 `getc/putc`：

| 输入 | 0.2.0 | 0.3.0 | 提升 |
|---|---|---|---|
| `simsun.ttc` 18 MB / 2 面 | 1 618 ms | **17 ms** | ≈ 95× |
| `mingliub.ttc` 37 MB / 4 面 | 6 899 ms | **76 ms** | ≈ 90× |
| 15 个系统字体合计 | ≈ 25 s | ≈ 0.3 s | — |

同机 `fread` + `fwrite` 的 I/O 下界是 18.6 ms（读 1 份、写 2 份），也就是说 0.3.0 已经贴着
磁盘带宽跑（≈ 1 GB/s，含页缓存）。

## 已知边界

- **只拆不装**。没有 `.ttf` → `.ttc` 的打包，也没有表去重；每个面独占一份表数据，所以输出
  总体积是输入的 1.7–4.0 倍（`simsun.ttc` 2.00×，`mingliub.ttc` 3.99×）。
- **不做子集化**。不裁剪 glyph、不合并 DSIG；v2 头里的 DSIG 字段不会带到输出面里。
- **表数据原样复制**，只重算校验和与偏移，不解码字形（这是刻意的：解码交给 `ttf2mesh` 或
  `shx_tools` 那类真正的解析器）。
- 只支持 TrueType Collection；`.woff` / `.woff2` / `.dfont` 不在范围内。
- 大集合一次性进内存时峰值约为 `面数 × 文件长度`，需要控制内存请用 `UnzipTTCFace()`。

## 构建与测试

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

测试不依赖仓库里的任何字体：合成夹具逐字节构造集合（含共享表、v2 头、CFF 面、畸形头部），
覆盖拆分正确性、命名、落盘、兼容接口与 12 类非法输入。若本机有真实字体，会自动追加一轮
"真字体校验"（逐表比对 + 整文件校验和）。当前本机结果：合成用例 291 项断言通过，15 个系统
`.ttc` 全部通过（每个字体的断言数随面数增加）。

指定字体：`TTC_TOOLS_TEST_FONT=/path/to/any.ttc`；默认依次尝试 `fonts/simsun.ttc`、
`fonts/msyh.ttc`、`C:/Windows/Fonts/simsun.ttc`。

> 仓库不附带字体文件。`.ttc` 多为厂商专有字体（例如中易宋体），仓库里只放代码。

## 与 shx_tools 的关系

`shx_tools` 做的是反方向：把 AutoCAD `.shx` 的形/字体解析成轮廓，再**写出** `.ttf` / `.otf`。
两者共用同一段 sfnt 知识（表目录、4 字节对齐、`0xB1B0AFBA` 校验和补偿），但一个写、一个拆。
`ttc_tools` 不依赖 `shx_tools`，也不与它共享代码——各自作为独立的三方库可以被单独取用。

## 许可

MIT，见 [LICENSE](LICENSE)。
