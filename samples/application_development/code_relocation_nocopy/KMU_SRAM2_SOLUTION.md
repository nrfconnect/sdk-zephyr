# nRF54L15 KMU段放置问题解决方案

## 测试环境
- **硬件**: nRF54L15DK (Nordic Development Kit)
- **软件**: NCS SDK v3.3.0-rc1

## 问题描述

在nRF54L15平台上使用`zephyr_code_relocate`功能时，遇到链接器错误：
```
The section NRF_KMU_RESERVED_PUSH_SECTION needs to be placed on the top RAM address but it is not
```

**根本原因**：
- nRF54L15的KMU（密钥管理单元）要求其保留段必须放置在RAM的起始地址（`0x20000000`）
- 使用`zephyr_code_relocate(FILES src/sram_code.c LOCATION RAM)`时，Zephyr的链接器系统会向RAM添加额外的段
- 链接器宏`ALIGN(_region_min_align)`会导致KMU段从`0x20000000`偏移到`0x20000020`
- 自定义MEMORY块和重定位代码段的组合破坏了KMU段的绝对地址要求

## 解决方案：独立的SRAM2区域

通过创建一个独立的SRAM2内存区域，将需要重定位的代码放在其中，避免干扰默认RAM区域中的KMU段。

### 1. 创建板级Overlay文件

`boards/nrf54l15dk_nrf54l15_cpuapp.overlay`:
```dts
/*
 * Overlay for nRF54L15 DK to carve out 4KB from SRAM for code relocation
 * This reduces the default RAM region from 188KB to 184KB, freeing up
 * a 4KB block at the top for SRAM2 region.
 */
/ {
	soc {
		compatible = "simple-bus";
		#address-cells = <1>;
		#size-cells = <1>;

		/* 将默认RAM从188KB减少到184KB */
		cpuapp_sram: memory@20000000 {
			reg = <0x20000000 0x2E000>; /* 184KB */
		};

		/* SRAM2区域：位于SRAM顶部的4KB */
		sram2: memory@2002E000 {
			compatible = "zephyr,memory-region", "mmio-sram";
			reg = <0x2002E000 0x1000>; /* 4KB */
			zephyr,memory-region = "SRAM2";
		};
	};
};
```

### 2. 修改链接器脚本

`linker_arm_nocopy.ld`:
```ld
#if defined(CONFIG_SOC_NRF54L15_CPUAPP)
/*
 * For nRF54L15, use a dedicated SRAM2 region for code relocation.
 * This keeps the KMU section at the exact RAM start address 0x20000000.
 * The board overlay reduces main RAM to 184KB, freeing up top 4KB for SRAM2.
 * SRAM2 region is defined in the board overlay, not here, to avoid
 * redefinition warning.
 */

MEMORY
{
	EXTFLASH (rx) : ORIGIN = EXTFLASH_ADDR, LENGTH = EXTFLASH_SIZE
}

#include <zephyr/arch/arm/cortex_m/scripts/linker.ld>

#else
/* For other platforms, use the original approach */
MEMORY
{
	EXTFLASH (rx) : ORIGIN = EXTFLASH_ADDR, LENGTH = EXTFLASH_SIZE
}

#include <zephyr/arch/arm/cortex_m/scripts/linker.ld>
#endif
```

**重要**：不要在这里定义SRAM2，让Zephyr从设备树自动生成，避免重复定义警告。

### 3. 修改CMakeLists.txt

`CMakeLists.txt`:
```cmake
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(code_relocation_nocopy)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# For nRF54L15, use SRAM2 region for code relocation
# SRAM2 is a 4KB region carved from the top of SRAM (0x2002E000 - 0x2002F000)
# This keeps KMU section at exact RAM start address 0x20000000
if(CONFIG_SOC_NRF54L15_CPUAPP)
  # Relocate sram_code.c to SRAM2 region (defined in overlay and linker script)
  zephyr_code_relocate(FILES src/sram_code.c LOCATION SRAM2)
else()
  # For other platforms, use default RAM location
  zephyr_code_relocate(FILES src/sram_code.c LOCATION RAM)
endif()
```

## 内存布局

```
SRAM内存映射 (nRF54L15):
-------------------------------------------
0x20000000: KMU保留段 (96字节)
0x20000060: Zephyr RAM区域 (184KB - 96字节)
0x2002E000: SRAM2区域 (4KB) - 用于代码重定位
0x2002F000: SRAM结束
```

## 验证结果

构建成功后，检查以下内容：

### 1. 内存使用情况
```
Memory region         Used Size  Region Size  %age Used
        EXTFLASH:          0 GB      1400 KB      0.00%
           SRAM2:         132 B         4 KB      3.22%
           FLASH:       40336 B      1428 KB      2.76%
             RAM:        8688 B       184 KB      4.61%
        IDT_LIST:          0 GB        32 KB      0.00%
```

### 2. KMU段位置
```bash
# 检查zephyr.map或zephyr.stat文件
NRF_KMU_RESERVED_PUSH_SECTION NOBITS 20000000
```

### 3. 重定位代码段位置
```bash
# sram_code.c中的函数和数据应该在SRAM2区域
.sram2_text_reloc  PROGBITS 2002e000  # 函数代码
.sram2_data_reloc  PROGBITS 2002e080  # 数据变量
```

## 关键要点

1. **KMU要求**：nRF54L15的KMU段必须绝对定位在`0x20000000`
2. **避免干扰**：代码重定位不应影响默认RAM区域的内存布局
3. **独立区域**：通过创建独立的SRAM2区域，解决KMU段与重定位代码的冲突
4. **内存划分**：从默认RAM中划出4KB作为SRAM2，确保两个区域不重叠
5. **设备树驱动**：使用设备树定义内存区域，避免链接器脚本中的重复定义

## 故障排除

### 警告处理
如果出现`warning: redeclaration of memory region 'SRAM2'`：
- 确保只在设备树中定义SRAM2，不要在链接器脚本中重复定义
- 清理构建目录后重新构建

### 构建失败
如果构建失败并提示KMU段位置错误：
1. 检查设备树overlay是否正确加载
2. 验证RAM区域大小是否正确减少（184KB）
3. 确认SRAM2区域地址和大小正确（0x2002E000, 4KB）

## 适用范围

此解决方案适用于：
- nRF54L15平台
- 需要代码重定位的项目
- 使用KMU或其他需要绝对地址定位的外设
- Zephyr RTOS v3.3.0及以上版本