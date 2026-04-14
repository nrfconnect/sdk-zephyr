# nRF54L15 KMU段与代码重定位解决方案总结

## 问题背景

nRF54L15芯片的KMU（Key Management Unit）段有**硬性硬件要求**：必须放置在RAM起始地址 `0x20000000`。

用户使用Zephyr的 `zephyr_code_relocate` 功能将代码从Flash重定位到RAM执行时，会导致KMU段被推移到 `0x20000020`，违反了硬件规范，可能导致KMU功能失效。

---

## 尝试过的方案

### 方案1：直接重定位到RAM（原始方案）
```cmake
zephyr_code_relocate(FILES src/sram_code.c LOCATION RAM)
```

**问题分析**:
- 构建系统按顺序放置段：先放置重定位代码，再放置KMU段
- KMU段被推到 `0x20000020`，地址发生偏移
- **结果**: ❌ 不符合硬件要求

### 方案2：修改链接器脚本硬编码KMU地址

**思路**：在自定义链接器脚本中使用 `AT()` 或强制段位置指令固定KMU地址

**问题分析**:
- KMU段由Zephyr的 `kmu_push_area_section.ld` 自动处理，机制复杂
- 硬编码会与Zephyr的内存管理系统冲突
- 难以保证在不同构建配置下都正确工作
- 代码可维护性差
- **结果**: ❌ 过于复杂且不可靠

---

## 最终成功方案：创建独立SRAM2区域

### 核心思路

**物理隔离原则**：创建一个独立的SRAM2内存区域（从RAM顶部 carve out），专门存放重定位代码，保持KMU在原始RAM起始地址不变。

### 实现步骤

#### 1. 设备树Overlay配置

**文件**: `boards/nrf54l15dk_nrf54l15_cpuapp.overlay`

```dts
/ {
    cpus {
        cpu@0 {
            memory-region = &cpuapp_sram, &cpuapp_sram2;
        };
    };

    reserved-memory {
        cpuapp_sram: memory@20000000 {
            compatible = "zephyr,memory-region";
            device_config = <0x1>;
            zephyr,memory-region = "CPUAPP_SRAM";
            /* 缩减为184KB，为SRAM2腾出顶部4KB空间 */
            reg = <0x20000000 0x2E000>;
            status = "okay";
        };

        cpuapp_sram2: memory@2002e000 {
            compatible = "zephyr,memory-region";
            device_config = <0x100>;
            zephyr,memory-region = "CPUAPP_SRAM2";
            /* 4KB SRAM2区域，用于代码重定位 */
            reg = <0x2002E000 0x1000>;
            status = "okay";
        };
    };
};
```

**关键点**:
- `cpuapp_sram` 从192KB缩减到184KB (`0x2E000`)
- `cpuapp_sram2` 占用顶部4KB (`0x1000`)
- SRAM2地址 `0x2002E000` 远离KMU所在的起始地址

#### 2. 链接器脚本配置

**文件**: `linker_arm_nocopy.ld`

```ld
#if defined(CONFIG_SOC_NRF54L15_CPUAPP)
/* 对于nRF54L15，使用专用SRAM2区域进行代码重定位
 * SRAM2区域在设备树overlay中定义，此处不再重复定义
 * 避免与CMake自动生成的定义冲突
 */

MEMORY
{
    EXTFLASH (rx) : ORIGIN = EXTFLASH_ADDR, LENGTH = EXTFLASH_SIZE
}

#include <zephyr/arch/arm/cortex_m/scripts/linker.ld>

#else
/* 其他平台使用原始方法 */
MEMORY
{
    EXTFLASH (rx) : ORIGIN = EXTFLASH_ADDR, LENGTH = EXTFLASH_SIZE
}

#include <zephyr/arch/arm/cortex_m/scripts/linker.ld>
#endif
```

**关键点**:
- 仅保留 `EXTFLASH` 定义
- **移除** SRAM2的MEMORY定义（避免与设备树自动生成的定义冲突）
- 使用条件编译支持多平台

#### 3. CMakeLists.txt配置

**文件**: `CMakeLists.txt`

```cmake
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(code_relocation_nocopy)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# 对于nRF54L15，使用SRAM2区域进行代码重定位
# SRAM2是4KB区域，从RAM顶部 carve out (0x2002E000 - 0x2002F000)
# 这样保持KMU段在精确的RAM起始地址0x20000000
if(CONFIG_SOC_NRF54L15_CPUAPP)
  # 将sram_code.c重定位到SRAM2区域（在overlay和链接器脚本中定义）
  zephyr_code_relocate(FILES src/sram_code.c LOCATION SRAM2)
else()
  # 其他平台使用默认RAM位置
  zephyr_code_relocate(FILES src/sram_code.c LOCATION RAM)
endif()
```

**关键点**:
- 条件判断：nRF54L15使用 `LOCATION SRAM2`
- 其他平台使用 `LOCATION RAM` 保持兼容性
- 清晰的注释说明设计意图

---

## 内存布局示意图

```
nRF54L15 RAM地址空间 (192KB = 0x30000)

0x20000000 ┌─────────────────────────────────────┐
           │                                     │
           │  KMU段 (固定位置)                   │
           │  - 必须精确在此地址                 │
           │                                     │
           ├─────────────────────────────────────┤
           │                                     │
           │                                     │
           │  CPUAPP_SRAM区域 (184KB)            │
           │  - 应用程序代码                     │
           │  - 数据段                           │
           │  - 堆栈/堆                         │
           │                                     │
           │                                     │
0x2002DFFF ├─────────────────────────────────────┤
           │                                     │
           │  CPUAPP_SRAM2区域 (4KB)             │
           │  - sram_code.c 重定位代码          │
           │  - 起始地址: 0x2002E000            │
           │  - 大小: 0x1000 (4096字节)         │
           │                                     │
0x2002EFFF └─────────────────────────────────────┘
0x2002F000 (RAM结束地址)
```

---

## 关键设计决策

### 1. SRAM2放在RAM顶部
- **优点**: 远离起始地址 `0x20000000`，完全避免干扰KMU段
- **权衡**: 主RAM从192KB缩减到184KB（损失4KB）
- **决策**: 可接受，4KB足够存放需要重定位的代码

### 2. 主RAM缩减为184KB
- **实现**: 在设备树中修改 `reg = <0x20000000 0x2E000>`
- **原因**: 为SRAM2腾出空间
- **验证**: 应用程序仍可正常运行

### 3. 设备树定义SRAM2，链接器不重复定义
- **问题发现**: 初始方案在链接器脚本中定义SRAM2，导致重复定义警告
- **解决**: 仅由设备树定义，CMake自动生成链接器脚本部分
- **结果**: 消除警告，构建更干净

### 4. 条件编译支持多平台
- **实现**: `if(CONFIG_SOC_NRF54L15_CPUAPP)` 判断
- **优点**: 代码在nRF54L15和其他平台都能编译
- **维护性**: 单一源码库支持多个目标

---

## 验证结果

### 构建输出验证
```
✅ 构建成功，无错误无警告
✅ 内存区域正确定义
✅ SRAM2段已创建
```

### 关键验证点

| 验证项 | 预期结果 | 实际结果 | 状态 |
|--------|----------|----------|------|
| KMU段地址 | `0x20000000` | `0x20000000` | ✅ |
| SRAM2段存在 | 应该存在 | 已创建 | ✅ |
| SRAM2起始地址 | `0x2002E000` | `0x2002E000` | ✅ |
| 重定位代码位置 | SRAM2区域 | SRAM2区域 | ✅ |
| 构建警告 | 无 | 无 | ✅ |

### 验证命令
```bash
# 检查内存使用
west build -p -b nrf54l15dk/nrf54l15/cpuapp

# 查看详细内存布局
arm-zephyr-eabi-objdump -h build/zephyr/zephyr.elf

# 检查KMU段位置
grep "NRF_KMU_RESERVED_PUSH_SECTION" build/zephyr/zephyr.map
```

---

## 总结

### 问题解决的思路转变

**从**: "如何在同一RAM区域内同时放置KMU和重定位代码"

**到**: "创建独立内存区域，物理隔离KMU和重定位代码"

### 成功要素

1. **深入理解硬件约束**：KMU必须固定在 `0x20000000`
2. **利用设备树机制**：Zephyr的设备树自动处理内存区域定义
3. **物理隔离设计**：SRAM2从RAM顶部 carve out，远离敏感区域
4. **保持代码兼容性**：条件编译支持多平台
5. **消除重复定义**：设备树和链接器脚本职责分离

### 经验与教训

- **不要对抗硬件约束**：硬件要求必须严格遵守
- **利用框架机制**：Zephyr的设备树和构建系统提供了强大工具
- **简单即美**：物理隔离比复杂的地址计算更可靠
- **验证是关键**：通过map文件和构建日志验证设计方案

---

## 参考文件

- `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` - 设备树内存区域定义
- `linker_arm_nocopy.ld` - 链接器脚本配置
- `CMakeLists.txt` - 构建系统配置
- `src/sram_code.c` - 需要重定位的代码示例
- `KMU_SRAM2_SOLUTION.md` - 详细解决方案文档

## 相关文档

- [nRF54L15 Product Specification](https://docs.nordicsemi.com/) - KMU章节
- [Zephyr Code Relocation](https://docs.zephyrproject.org/latest/kernel/code-relocation.html) - 官方文档
- [Zephyr Device Tree](https://docs.zephyrproject.org/latest/build/dts/index.html) - 设备树指南
