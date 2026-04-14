# nRF54L15 UART21 跨电源域配置总结

## 测试环境
- **硬件**: nRF54L15DK (Nordic Development Kit)
- **软件**: NCS SDK v3.3.0-rc1

## 目标
将 hello_world 的 UART 输出从默认的 UART20 (P1.04/P1.05) 切换到 UART21 (P2.08/P2.07)。

## 关键问题
UARTE21 属于 **PERI 域**，P2 属于 **MCU 域**。跨电源域使用 UART 必须启用 **Constant Latency** 子电源模式。

## 配置步骤

### 1. DTS Overlay (`boards/nrf54l15dk_nrf54l15_cpuapp.overlay`)
```dts
/ {
    chosen {
        zephyr,console = &uart21;
        zephyr,shell-uart = &uart21;
    };
};

&pinctrl {
    uart21_default: uart21_default {
        group1 {
            psels = <NRF_PSEL(UART_TX, 2, 8)>,
                    <NRF_PSEL(UART_RX, 2, 7)>;
        };
    };

    uart21_sleep: uart21_sleep {
        group1 {
            psels = <NRF_PSEL(UART_TX, 2, 8)>,
                    <NRF_PSEL(UART_RX, 2, 7)>;
            low-power-enable;
        };
    };
};

&uart21 {
    status = "okay";
    current-speed = <115200>;
    pinctrl-names = "default", "sleep";
    pinctrl-0 = <&uart21_default>;
    pinctrl-1 = <&uart21_sleep>;
};
```

### 2. prj.conf
```conf
CONFIG_NRF_SYS_EVENT=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_SERIAL=y
```

### 3. main.c
```c
#include <nrf_sys_event.h>

int main(void)
{
    /* 请求 Constant Latency 模式以支持跨电源域 UART */
    nrf_sys_event_request_global_constlat();

    printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

    return 0;
}
```

## 验证结果

| 项目 | 配置 | 验证状态 |
|------|------|----------|
| UART 实例 | uart21 | ✅ |
| TXD 引脚 | **P2.08** | ✅ 逻辑分析仪确认 |
| RXD 引脚 | **P2.07** | ✅ |
| 波特率 | 115200 | ✅ 波形验证 |
| 跨域模式 | Constant Latency | ✅ 必需 |

## 核心要点

- **跨域必须启用 Constant Latency 模式**：`nrf_sys_event_request_global_constlat()`
- **功耗注意**：该模式会增加功耗，使用完毕后应调用 `nrf_sys_event_release_global_constlat()` 释放
- **硬件验证**：使用逻辑分析仪在 P2.08 捕获到正确的 115200 波特率波形

## 参考文件

- `boards/nrf54l15dk_nrf54l15_cpuapp.overlay` - 设备树配置
- `prj.conf` - 项目配置
- `src/main.c` - 应用代码
