# H.264 NAL 单元格式说明

## 概述

H.264/AVC 编码的视频流由一系列 NAL（Network Abstraction Layer）单元组成。每个 NAL 单元封装一种类型的编码数据，如序列参数集（SPS）、图像参数集（PPS）或编码片（Slice）。

## NAL 单元结构

### 字节流格式

```
[起始码] [NAL 头部] [NAL 载荷]
```

- **起始码**: `0x00 0x00 0x01` (3 字节) 或 `0x00 0x00 0x00 0x01` (4 字节)
- **NAL 头部**: 1 字节

### NAL 头部（1 字节）

```
位:  7        5-6       0-4
    +----+-----------+-----------+
    |  F |   NRI     |   Type    |
    +----+-----------+-----------+

F (forbidden_zero_bit):  1 bit，必须为 0
NRI (nal_ref_idc):       2 bits，非 0 表示该 NAL 属于参考图像
Type (nal_unit_type):    5 bits，NAL 单元类型
```

## NAL 单元类型

| 类型 | 值 | 说明 |
|------|-----|------|
| 未指定 | 0 | Unspecified |
| 非 IDR 片 | 1 | Coded slice of a non-IDR picture |
| 片数据分区 A | 2 | Coded slice data partition A |
| 片数据分区 B | 3 | Coded slice data partition B |
| 片数据分区 C | 4 | Coded slice data partition C |
| IDR 片 | 5 | Coded slice of an IDR picture |
| SEI | 6 | Supplemental enhancement information |
| SPS | 7 | Sequence parameter set |
| PPS | 8 | Picture parameter set |
| AUD | 9 | Access unit delimiter |
| 序列结束 | 10 | End of sequence |
| 流结束 | 11 | End of stream |
| 填充 | 12 | Filler data |

## SPS（序列参数集）

SPS 包含解码整个视频序列所需的全局参数。

### 关键字段

| 字段 | 说明 |
|------|------|
| `profile_idc` | 编码配置文件 (66=Baseline, 77=Main, 100=High) |
| `level_idc` | 编码级别 (10=1.0, 30=3.0, 50=5.0) |
| `pic_width_in_mbs_minus1` | 图像宽度（宏块数 - 1） |
| `pic_height_in_map_units_minus1` | 图像高度（宏块数 - 1） |
| `frame_mbs_only_flag` | 仅帧编码模式 |
| `frame_cropping_flag` | 帧裁剪标志 |

### 分辨率计算

```c
width  = (pic_width_in_mbs_minus1 + 1) * 16;
height = (pic_height_in_map_units_minus1 + 1) * 16;

if (frame_cropping_flag) {
    width  -= crop_left + crop_right;
    height -= crop_top + crop_bottom;
}

if (!frame_mbs_only_flag) {
    height *= 2;  /* 场编码 */
}
```

## PPS（图像参数集）

PPS 包含每一帧（或几帧）的编码参数。

### 关键字段

| 字段 | 说明 |
|------|------|
| `entropy_coding_mode_flag` | 0=CAVLC, 1=CABAC |
| `num_ref_idx_l0_active_minus1` | L0 参考帧数量 |
| `weighted_pred_flag` | 加权预测 |
| `pic_init_qp_minus26` | 初始 QP - 26 |
| `deblocking_filter_control_present_flag` | 去块滤波 |

## 片头（Slice Header）

片头包含一个片（Slice）的解码参数。

### 关键字段

| 字段 | 说明 |
|------|------|
| `first_mb_in_slice` | 片内第一个宏块的地址 |
| `slice_type` | 片类型 (0=P, 1=B, 2=I) |
| `frame_num` | 帧编号 |
| `pic_order_cnt_lsb` | 图像顺序计数（LSB） |

### 片类型

| 类型 | 值 | 说明 |
|------|-----|------|
| P-Slice | 0 | 前向预测片 |
| B-Slice | 1 | 双向预测片 |
| I-Slice | 2 | 帧内预测片 |
| SP-Slice | 3 | 切换 P 片 |
| SI-Slice | 4 | 切换 I 片 |

## 指数哥伦布编码（Exp-Golomb）

H.264 中使用 ue(v)（无符号）和 se(v)（有符号）指数哥伦布编码变长语法元素。

### 无符号 (ue)

```
1. 计数前导 0 的个数 L
2. 读出 L 位信息位
3. value = (1 << L) - 1 + info
```

### 有符号 (se)

```
value = (ue + 1) >> 1  (奇数)
value = -(ue >> 1)     (偶数)
```

## 参考

- ITU-T H.264 (2021-08)
- ISO/IEC 14496-10:2020
