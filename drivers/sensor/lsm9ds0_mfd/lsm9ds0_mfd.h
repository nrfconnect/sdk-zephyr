/* sensor_lsm9ds0_mfd.h - header file for LSM9DS0 accelerometer, magnetometer
 * and temperature (MFD) sensor driver
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_LSM9DS0_MFD_LSM9DS0_MFD_H_
#define ZEPHYR_DRIVERS_SENSOR_LSM9DS0_MFD_LSM9DS0_MFD_H_

#include <zephyr/types.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/i2c.h>

#define LSM9DS0_MFD_REG_OUT_TEMP_L_XM		0x05
#define LSM9DS0_MFD_REG_OUT_TEMP_H_XM		0x06

#define LSM9DS0_MFD_REG_STATUS_REG_M            0x07
#define LSM9DS0_MFD_MASK_STATUS_REG_M_ZYXMOR    BIT(7)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_M_ZYXMOR   7
#define LSM9DS0_MFD_MASK_STATUS_REG_M_ZMOR      BIT(6)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_M_ZMOR     6
#define LSM9DS0_MFD_MASK_STATUS_REG_M_YMOR      BIT(5)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_M_YMOR     5
#define LSM9DS0_MFD_MASK_STATUS_REG_M_XMOR      BIT(4)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_M_XMOR     4
#define LSM9DS0_MFD_MASK_STATUS_REG_M_ZYXMDA    BIT(3)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_M_ZYXMDA   3
#define LSM9DS0_MFD_MASK_STATUS_REG_M_ZMDA      BIT(2)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_M_ZMDA     2
#define LSM9DS0_MFD_MASK_STATUS_REG_M_YMDA      BIT(1)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_M_YMDA     1
#define LSM9DS0_MFD_MASK_STATUS_REG_M_XMDA      BIT(0)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_XMDA       0

#define LSM9DS0_MFD_REG_OUT_X_L_M               0x08
#define LSM9DS0_MFD_REG_OUT_X_H_M               0x09
#define LSM9DS0_MFD_REG_OUT_Y_L_M               0x0A
#define LSM9DS0_MFD_REG_OUT_Y_H_M               0x0B
#define LSM9DS0_MFD_REG_OUT_Z_L_M               0x0C
#define LSM9DS0_MFD_REG_OUT_Z_H_M               0x0D

#define LSM9DS0_MFD_REG_WHO_AM_I_XM             0x0F
#define LSM9DS0_MFD_VAL_WHO_AM_I_XM             0x49

#define LSM9DS0_MFD_REG_INT_CTRL_REG_M          0x12
#define LSM9DS0_MFD_MASK_INT_CTRL_REG_M_XMIEN   BIT(7)
#define LSM9DS0_MFD_SHIFT_INT_CTRL_REG_M_XMIEN  7
#define LSM9DS0_MFD_MASK_INT_CTRL_REG_M_YMIEN   BIT(6)
#define LSM9DS0_MFD_SHIFT_INT_CTRL_REG_M_YMIEN  6
#define LSM9DS0_MFD_MASK_INT_CTRL_REG_M_ZMIEN   BIT(5)
#define LSM9DS0_MFD_SHIFT_INT_CTRL_REG_M_ZMIEN  5
#define LSM9DS0_MFD_MASK_INT_CTRL_REG_M_PP_OD   BIT(4)
#define LSM9DS0_MFD_SHIFT_INT_CTRL_REG_M_PP_OD  4
#define LSM9DS0_MFD_MASK_INT_CTRL_REG_M_IEA     BIT(3)
#define LSM9DS0_MFD_SHIFT_INT_CTRL_REG_M_IEA    3
#define LSM9DS0_MFD_MASK_INT_CTRL_REG_M_IEL     BIT(2)
#define LSM9DS0_MFD_SHIFT_INT_CTRL_REG_M_IEL    2
#define LSM9DS0_MFD_MASK_INT_CTRL_REG_M_4D      BIT(1)
#define LSM9DS0_MFD_SHIFT_INT_CTRL_REG_M_4D     1
#define LSM9DS0_MFD_MASK_INT_CTRL_REG_M_MIEN    BIT(0)
#define LSM9DS0_MFD_SHIFT_INT_CTRL_REG_M_MIEN   0

#define LSM9DS0_MFD_REG_INT_SRC_REG_M           0x13
#define LSM9DS0_MFD_MASK_INT_SRC_REG_M_M_PTH_X  BIT(7)
#define LSM9DS0_MFD_SHIFT_INT_SRC_REG_M_M_PTH_X 7
#define LSM9DS0_MFD_MASK_INT_SRC_REG_M_M_PTH_Y  BIT(6)
#define LSM9DS0_MFD_SHIFT_INT_SRC_REG_M_M_PTH_Y 6
#define LSM9DS0_MFD_MASK_INT_SRC_REG_M_M_PTH_Z  BIT(5)
#define LSM9DS0_MFD_SHIFT_INT_SRC_REG_M_M_PTH_Z 5
#define LSM9DS0_MFD_MASK_INT_SRC_REG_M_M_NTH_X  BIT(4)
#define LSM9DS0_MFD_SHIFT_INT_SRC_REG_M_M_NTH_X 4
#define LSM9DS0_MFD_MASK_INT_SRC_REG_M_M_NTH_Y  BIT(3)
#define LSM9DS0_MFD_SHIFT_INT_SRC_REG_M_M_NTH_Y 3
#define LSM9DS0_MFD_MASK_INT_SRC_REG_M_M_NTH_Z  BIT(2)
#define LSM9DS0_MFD_SHIFT_INT_SRC_REG_M_M_NTH_Z 2
#define LSM9DS0_MFD_MASK_INT_SRC_REG_M_MROI     BIT(1)
#define LSM9DS0_MFD_SHIFT_INT_SRC_REG_M_MROI    1
#define LSM9DS0_MFD_MASK_INT_SRC_REG_M_MINT     BIT(0)
#define LSM9DS0_MFD_SHIFT_INT_SRC_REG_M_MINT    0

#define LSM9DS0_MFD_REG_INT_THS_L_M             0x14
#define LSM9DS0_MFD_REG_INT_THS_H_M             0x15
#define LSM9DS0_MFD_REG_OFFSET_X_L_M            0x16
#define LSM9DS0_MFD_REG_OFFSET_X_H_M            0x17
#define LSM9DS0_MFD_REG_OFFSET_Y_L_M            0x18
#define LSM9DS0_MFD_REG_OFFSET_Y_H_M            0x19
#define LSM9DS0_MFD_REG_OFFSET_Z_L_M            0x1A
#define LSM9DS0_MFD_REG_OFFSET_Z_H_M            0x1B

#define LSM9DS0_MFD_REG_REFERENCE_X             0x1C
#define LSM9DS0_MFD_REG_REFERENCE_Y             0x1D
#define LSM9DS0_MFD_REG_REFERENCE_Z             0x1E

#define LSM9DS0_MFD_REG_CTRL_REG0_XM            0x1F
#define LSM9DS0_MFD_MASK_CTRL_REG0_XM_BOOT      BIT(7)
#define LSM9DS0_MFD_SHIFT_CTRL_REG0_XM_BOOT     7
#define LSM9DS0_MFD_MASK_CTRL_REG0_XM_FIFO_EN   BIT(6)
#define LSM9DS0_MFD_SHIFT_CTRL_REG0_XM_FIFO_EN  6
#define LSM9DS0_MFD_MASK_CTRL_REG0_XM_WTM_EN    BIT(5)
#define LSM9DS0_MFD_SHIFT_CTRL_REG0_XM_WTM_EN   5
#define LSM9DS0_MFD_MASK_CTRL_REG0_XM_HP_C      BIT(2)
#define LSM9DS0_MFD_SHIFT_CTRL_REG0_XM_HP_C     2
#define LSM9DS0_MFD_MASK_CTRL_REG0_XM_HPIS1     BIT(1)
#define LSM9DS0_MFD_SHIFT_CTRL_REG0_XM_HPIS1    1
#define LSM9DS0_MFD_MASK_CTRL_REG0_XM_HPIS2     BIT(0)
#define LSM9DS0_MFD_SHIFT_CTRL_REG0_XM_HPIS2    0

#define LSM9DS0_MFD_REG_CTRL_REG1_XM            0x20
#define LSM9DS0_MFD_MASK_CTRL_REG1_XM_AODR      (BIT(7) | BIT(6) | BIT(5) | \
						 BIT(4))
#define LSM9DS0_MFD_SHIFT_CTRL_REG1_XM_AODR     4
#define LSM9DS0_MFD_MASK_CTRL_REG1_XM_BDU       BIT(3)
#define LSM9DS0_MFD_SHIFT_CTRL_REG1_XM_BDU      3
#define LSM9DS0_MFD_MASK_CTRL_REG1_XM_AZEN      BIT(2)
#define LSM9DS0_MFD_SHIFT_CTRL_REG1_XM_AZEN     2
#define LSM9DS0_MFD_MASK_CTRL_REG1_XM_AYEN      BIT(1)
#define LSM9DS0_MFD_SHIFT_CTRL_REG1_XM_AYEN     1
#define LSM9DS0_MFD_MASK_CTRL_REG1_XM_AXEN      BIT(0)
#define LSM9DS0_MFD_SHIFT_CTRL_REG1_XM_AXEN     0

#define LSM9DS0_MFD_REG_CTRL_REG2_XM            0x21
#define LSM9DS0_MFD_MASK_CTRL_REG2_XM_ABW       (BIT(7) | BIT(6))
#define LSM9DS0_MFD_SHIFT_CTRL_REG2_XM_ABW      6
#define LSM9DS0_MFD_MASK_CTRL_REG2_XM_AFS       (BIT(5) | BIT(4) | BIT(3))
#define LSM9DS0_MFD_SHIFT_CTRL_REG2_XM_AFS      3
#define LSM9DS0_MFD_MASK_CTRL_REG2_XM_AST       (BIT(2) | BIT(1))
#define LSM9DS0_MFD_SHIFT_CTRL_REG2_XM_AST      1
#define LSM9DS0_MFD_MASK_CTRL_REG2_XM_SIM       BIT(0)
#define LSM9DS0_MFD_SHIFT_CTRL_REG2_XM_SIM      0

#define LSM9DS0_MFD_REG_CTRL_REG3_XM            0x22
#define LSM9DS0_MFD_MASK_CTRL_REG3_XM_P1_BOOT   BIT(7)
#define LSM9DS0_MFD_SHIFT_CTRL_REG3_XM_P1_BOOT  7
#define LSM9DS0_MFD_MASK_CTRL_REG3_XM_P1_TAP    BIT(6)
#define LSM9DS0_MFD_SHIFT_CTRL_REG3_XM_P1_TAP   6
#define LSM9DS0_MFD_MASK_CTRL_REG3_XM_P1_INT1   BIT(5)
#define LSM9DS0_MFD_SHIFT_CTRL_REG3_XM_P1_INT1  5
#define LSM9DS0_MFD_MASK_CTRL_REG3_XM_P1_INT2   BIT(4)
#define LSM9DS0_MFD_SHIFT_CTRL_REG3_XM_P1_INT2  4
#define LSM9DS0_MFD_MASK_CTRL_REG3_XM_P1_INTM   BIT(3)
#define LSM9DS0_MFD_SHIFT_CTRL_REG3_XM_P1_INTM  3
#define LSM9DS0_MFD_MASK_CTRL_REG3_XM_P1_DRDYA  BIT(2)
#define LSM9DS0_MFD_SHIFT_CTRL_REG3_XM_P1_DRDYA 2
#define LSM9DS0_MFD_MASK_CTRL_REG3_XM_P1_DRDYM  BIT(1)
#define LSM9DS0_MFD_SHIFT_CTRL_REG3_XM_P1_DRDYM 1
#define LSM9DS0_MFD_MASK_CTRL_REG3_XM_P1_EMPTY  BIT(0)
#define LSM9DS0_MFD_SHIFT_CTRL_REG3_XM_P1_EMPTY 0

#define LSM9DS0_MFD_REG_CTRL_REG4_XM            0x23
#define LSM9DS0_MFD_MASK_CTRL_REG4_XM_P2_TAP    BIT(7)
#define LSM9DS0_MFD_SHIFT_CTRL_REG4_XM_P2_TAP   7
#define LMS9DS0_MFD_MASK_CTRL_REG4_XM_P2_INT1   BIT(6)
#define LSM9DS0_MFD_SHIFT_CTRL_REG4_XM_P2_INT1  6
#define LSM9DS0_MFD_MASK_CTRL_REG4_XM_P2_INT2   BIT(5)
#define LSM9DS0_MFD_SHIFT_CTRL_REG4_XM_P2_INT2  5
#define LSM9DS0_MFD_MASK_CTRL_REG4_XM_P2_INTM   BIT(4)
#define LSM9DS0_MFD_SHIFT_CTRL_REG4_XM_P2_INTM  4
#define LSM9DS0_MFD_MASK_CTRL_REG4_XM_P2_DRDYA  BIT(3)
#define LSM9DS0_MFD_SHIFT_CTRL_REG4_XM_P2_DRDYA 3
#define LSM9DS0_MFD_MASK_CTRL_REG4_XM_P2_DRDYM  BIT(2)
#define LSM9DS0_MFD_SHIFT_CTRL_REG4_XM_P2_DRDYM 2
#define LSM9DS0_MFD_MASK_CTRL_REG4_XM_P2_OVR    BIT(1)
#define LSM9DS0_MFD_SHIFT_CTRL_REG4_XM_P2_OVR   1
#define LSM9DS0_MFD_MASK_CTRL_REG4_XM_P2_WTM    BIT(0)
#define LSM9DS0_MFD_SHIFT_CTRL_REG4_XM_P2_WTM   0

#define LSM9DS0_MFD_REG_CTRL_REG5_XM            0x24
#define LSM9DS0_MFD_MASK_CTRL_REG5_XM_TEMP_EN   BIT(7)
#define LSM9DS0_MFD_SHIFT_CTRL_REG5_XM_TEMP_EN  7
#define LSM9DS0_MFD_MASK_CTRL_REG5_XM_M_RES     (BIT(6) | BIT(5))
#define LSM9DS0_MFD_SHIFT_CTRL_REG5_XM_M_RES    5
#define LSM9DS0_MFD_MASK_CTRL_REG5_XM_M_ODR     (BIT(4) | BIT(3) | BIT(2))
#define LSM9DS0_MFD_SHIFT_CTRL_REG5_XM_M_ODR    2
#define LSM9DS0_MFD_MASK_CTRL_REG5_XM_LIR2      BIT(1)
#define LSM9DS0_MFD_SHIFT_CTRL_REG5_XM_LIR2     1
#define LSM9DS0_MFD_MASK_CTRL_REG5_XM_LIR1      BIT(0)
#define LSM9DS0_MFD_SHIFT_CTRL_REG5_XM_LIR1     0

#define LSM9DS0_MFD_REG_CTRL_REG6_XM            0x25
#define LSM9DS0_MFD_MASK_CTRL_REG6_XM_MFS       (BIT(6) | BIT(5))
#define LSM9DS0_MFD_SHIFT_CTRL_REG6_XM_MFS      5

#define LSM9DS0_MFD_REG_CTRL_REG7_XM            0x26
#define LSM9DS0_MFD_MASK_CTRL_REG7_XM_AHPM      (BIT(7) | BIT(6))
#define LSM9DS0_MFD_SHIFT_CTRL_REG7_XM_AHPM     6
#define LSM9DS0_MFD_MASK_CTRL_REG7_XM_AFDS      BIT(5)
#define LSM9DS0_MFD_SHIFT_CTRL_REG7_XM_AFDS     5
#define LSM9DS0_MFD_MASK_CTRL_REG7_XM_MLP       BIT(2)
#define LSM9DS0_MFD_SHIFT_CTRL_REG7_XM_MLP      2
#define LSM9DS0_MFD_MASK_CTRL_REG7_XM_MD        (BIT(1) | BIT(0))
#define LSM9DS0_MFD_SHIFT_CTRL_REG7_XM_MD       0

#define LSM9DS0_MFD_REG_STATUS_REG_A            0x27
#define LSM9DS0_MFD_MASK_STATUS_REG_A_ZYXAOR    BIT(7)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_A_ZYXAOR   7
#define LSM9DS0_MFD_MASK_STATUS_REG_A_ZAOR      BIT(6)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_A_ZAOR     6
#define LSM9DS0_MFD_MASK_STATUS_REG_A_YAOR      BIT(5)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_A_YAOR     5
#define LSM9DS0_MFD_MASK_STATUS_REG_A_XAOR      BIT(4)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_A_XAOR     4
#define LSM9DS0_MFD_MASK_STATUS_REG_A_ZYXADA    BIT(3)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_A_ZYXADA   3
#define LSM9DS0_MFD_MASK_STATUS_REG_A_ZADA      BIT(2)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_A_ZADA     2
#define LSM9DS0_MFD_MASK_STATUS_REG_A_YADA      BIT(1)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_A_YADA     1
#define LSM9DS0_MFD_MASK_STATUS_REG_A_XADA      BIT(0)
#define LSM9DS0_MFD_SHIFT_STATUS_REG_A_XADA     0

#define LSM9DS0_MFD_REG_OUT_X_L_A               0x28
#define LSM9DS0_MFD_REG_OUT_X_H_A               0x29
#define LSM9DS0_MFD_REG_OUT_Y_L_A               0x2A
#define LSM9DS0_MFD_REG_OUT_Y_H_A               0x2B
#define LSM9DS0_MFD_REG_OUT_Z_L_A               0x2C
#define LSM9DS0_MFD_REG_OUT_Z_H_A               0x2D

#define LSM9DS0_MFD_REG_FIFO_CTRL_REG           0x2E
#define LSM9DS0_MFD_MASK_FIFO_CTRL_REG_FM       (BIT(7) | BIT(6) | BIT(5))
#define LSM9DS0_MFD_SHIFT_FIFO_CTRL_REG_FM      5
#define LSM9DS0_MFD_MASK_FIFO_CTRL_REG_FTH      (BIT(4) | BIT(3) | BIT(2) | \
						 BIT(1) | BIT(0))
#define LSM9DS0_MFD_SHIFT_FIFO_CTRL_REG_FTH     0

#define LSM9DS0_MFD_REG_FIFO_SRC_REG            0x2F
#define LSM9DS0_MFD_MASK_FIFO_SRC_REG_WTM       BIT(7)
#define LMS9DS0_MFD_SHIFT_FIFO_SRC_REG_WTM      7
#define LSM9DS0_MFD_MASK_FIFO_SRC_REG_OVRN      BIT(6)
#define LSM9DS0_MFD_SHIFT_FIFO_SRC_REG_OVRN     6
#define LSM9DS0_MFD_MASK_FIFO_SRC_REG_EMPTY     BIT(5)
#define LMS9DS0_MFD_SHIFT_FIFO_SRC_REG_EMPTY    5
#define LSM9DS0_MFD_MASK_FIFO_SRC_REG_FSS       (BIT(4) | BIT(3) | BIT(2) | \
						 BIT(1) | BIT(0))
#define LSM9DS0_MFD_SHIFT_FIFO_SRC_REG_FSS      0

#define LSM9DS0_MFD_REG_INT_GEN_1_REG           0x30
#define LSM9DS0_MFD_MASK_INT_GEN_1_REG_AOI      BIT(7)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_REG_AOI     7
#define LSM9DS0_MFD_MASK_INT_GEN_1_REG_6D       BIT(6)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_REG_6D      6
#define LSM9DS0_MFD_MASK_INT_GEN_1_REG_ZHIE     BIT(5)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_REG_ZHIE    5
#define LSM9DS0_MFD_MASK_INT_GEN_1_REG_ZLIE     BIT(4)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_REG_ZLIE    4
#define LSM9DS0_MFD_MASK_INT_GEN_1_REG_YHIE     BIT(3)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_REG_YHIE    3
#define LSM9DS0_MFD_MASK_INT_GEN_1_REG_YLIE     BIT(2)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_REG_YLIE    2
#define LSM9DS0_MFD_MASK_INT_GEN_1_REG_XHIE     BIT(1)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_REG_XHIE    1
#define LSM9DS0_MFD_MASK_INT_GEN_1_REG_XLIE     BIT(0)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_REG_XLIE    0

#define LSM9DS0_MFD_REG_INT_GEN_1_SRC           0x31
#define LSM9DS0_MFD_MASK_INT_GEN_1_SRC_IA       BIT(6)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_SRC_IA      6
#define LSM9DS0_MFD_MASK_INT_GEN_1_SRC_ZH       BIT(5)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_SRC_ZH      5
#define LSM9DS0_MFD_MASK_INT_GEN_1_SRC_ZL       BIT(4)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_SRC_ZL      4
#define LSM9DS0_MFD_MASK_INT_GEN_1_SRC_YH       BIT(3)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_SRC_YH      3
#define LSM9DS0_MFD_MASK_INT_GEN_1_SRC_YL       BIT(2)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_SRC_YL      2
#define LSM9DS0_MFD_MASK_INT_GEN_1_SRC_XH       BIT(1)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_SRC_XH      1
#define LSM9DS0_MFD_MASK_INT_GEN_1_SRC_XL       BIT(0)
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_SRC_XL      0

#define LSM9DS0_MFD_REG_INT_GEN_1_THS           0x32
#define LSM9DS0_MFD_MASK_INT_GEN_1_THS_THS      (BIT(6) | BIT(5) | BIT(4) | \
						 BIT(3) | BIT(2) | BIT(1) | \
						 BIT(0))
#define LSM9DS0_MFD_SHIFT_INT_GEN_1_THS_THS     0

#define LSM9DS0_MFD_REG_INT_GEN_1_DURATION      0x33
#define LSM9DS0_MFD_MASK_INT_GEN_1_DURATION_D   (BIT(6) | BIT(5) | BIT(4) | \
						 BIT(3) | BIT(2) | BIT(1) | \
						 BIT(0))
#define LMS9DS0_MFD_SHIFT_INT_GEN_1_DURATION_D  0

#define LSM9DS0_MFD_REG_INT_GEN_2_REG           0x34
#define LSM9DS0_MFD_MASK_INT_GEN_2_REG_AOI      BIT(7)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_REG_AOI     7
#define LSM9DS0_MFD_MASK_INT_GEN_2_REG_6D       BIT(6)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_REG_6D      6
#define LSM9DS0_MFD_MASK_INT_GEN_2_REG_ZHIE     BIT(5)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_REG_ZHIE    5
#define LSM9DS0_MFD_MASK_INT_GEN_2_REG_ZLIE     BIT(4)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_REG_ZLIE    4
#define LSM9DS0_MFD_MASK_INT_GEN_2_REG_YHIE     BIT(3)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_REG_YHIE    3
#define LSM9DS0_MFD_MASK_INT_GEN_2_REG_YLIE     BIT(2)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_REG_YLIE    2
#define LSM9DS0_MFD_MASK_INT_GEN_2_REG_XHIE     BIT(1)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_REG_XHIE    1
#define LSM9DS0_MFD_MASK_INT_GEN_2_REG_XLIE     BIT(0)
#define LMS9Ds0_MFD_SHIFT_INT_GEN_2_REG_XLIE    0

#define LSM9DS0_MFD_REG_INT_GEN_2_SRC           0x35
#define LSM9DS0_MFD_MASK_INT_GEN_2_SRC_IA       BIT(6)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_SRC_IA      6
#define LSM9DS0_MFD_MASK_INT_GEN_2_SRC_ZH       BIT(5)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_SRC_ZH      5
#define LSM9DS0_MFD_MASK_INT_GEN_2_SRC_ZL       BIT(4)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_SRC_ZL      4
#define LSM9DS0_MFD_MASK_INT_GEN_2_SRC_YH       BIT(3)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_SRC_YH      3
#define LSM9DS0_MFD_MASK_INT_GEN_2_SRC_YL       BIT(2)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_SRC_YL      2
#define LSM9DS0_MFD_MASK_INT_GEN_2_SRC_XH       BIT(1)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_SRC_XH      1
#define LSM9DS0_MFD_MASK_INT_GEN_2_SRC_XL       BIT(0)
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_SRC_XL      0

#define LSM9DS0_MFD_REG_INT_GEN_2_THS           0x36
#define LSM9DS0_MFD_MASK_INT_GEN_2_THS_THS      (BIT(6) | BIT(5) | BIT(4) | \
						 BIT(3) | BIT(2) | BIT(1) | \
						 BIT(0))
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_THS_THS     0

#define LSM9DS0_MFD_REG_INT_GEN_2_DURATION      0x37
#define LSM9DS0_MFD_MASK_INT_GEN_2_DURATION_D   (BIT(6) | BIT(5) | BIT(4) | \
						 BIT(3) | BIT(2) | BIT(1) | \
	ensor_				 BIT(0))
#define LSM9DS0_MFD_SHIFT_INT_GEN_2_DURATION_D  0

#define LSM9DS0_MFD_REG_CLICK_CFG               0x38
#define LSM9DS0_MFD_MASK_CLICK_CFG_ZD           BIT(5)
#define LSM9DS0_MFD_SHIFT_CLICK_CFG_ZD          5
#define LSM9DS0_MFD_MASK_CLICK_CFG_ZS           BIT(4)
#define LSM9DS0_MFD_SHIFT_CLICK_CFG_ZS          4
#define LSM9DS0_MFD_MASK_CLICK_CFG_YD           BIT(3)
#define LSM9DS0_MFD_SHIFT_CLICK_CFG_YD          3
#define LSM9DS0_MFD_MASK_CLICK_CFG_YS           BIT(2)
#define LSM9DS0_MFD_SHIFT_CLICK_CFG_YS          2
#define LSM9DS0_MFD_MASK_CLICK_CFG_XD           BIT(1)
#define LSM9DS0_MFD_SHIFT_CLICK_CFG_XD          1
#define LSM9DS0_MFD_MASK_CLICK_CFG_XS           BIT(0)
#define LSM9DS0_MFD_SHIFT_CLICK_CFG_XS          0

#define LSM9DS0_MFD_REG_CLICK_SRC               0x39
#define LSM9DS0_MFD_MASK_CLICK_SRC_IA           BIT(6)
#define LSM9DS0_MFD_SHIFT_CLICK_SRC_IA          6
#define LSM9DS0_MFD_MASK_CLICK_SRC_DC           BIT(5)
#define LMS9DS0_MFD_SHIFT_CLICK_SRC_DC          5
#define LSM9DS0_MFD_MASK_CLICK_SRC_SC           BIT(4)
#define LSM9DS0_MFD_SHIFT_CLICK_SRC_SC          4
#define LSM9DS0_MFD_MASK_CLICK_SRC_S            BIT(3)
#define LSM9DS0_MFD_SHIFT_CLICK_SRC_S           3
#define LSM9DS0_MFD_MASK_CLICK_SRC_Z            BIT(2)
#define LSM9DS0_MFD_SHIFT_CLICK_SRC_Z           2
#define LSM9DS0_MFD_MASK_CLICK_SRC_Y            BIT(1)
#define LSM9DS0_MFD_SHIFT_CLICK_SRC_Y           1
#define LSM9DS0_MFD_MASK_CLICK_SRC_X            BIT(0)
#define LSM9DS0_MFD_SHIFT_CLICK_SRC_X           0

#define LSM9DS0_MFD_REG_CLICK_THS               0x3A
#define LSM9DS0_MFD_MASK_CLICK_THS_THS          (BIT(6) | BIT(5) | BIT(4) | \
						 BIT(3) | BIT(2) | BIT(1) | \
						 BIT(0))
#define LSM9DS0_MFD_SHIFT_CLICK_THS_THS         0

#define LSM9DS0_MFD_REG_TIME_LIMIT              0x3B
#define LSM9DS0_MFD_MASK_TIME_LIMIT_TLI         (BIT(6) | BIT(5) | BIT(4) | \
						 BIT(3) | BIT(2) | BIT(1) | \
						 BIT(0))
#define LMS9DS0_MFD_SHIFT_TIME_LIMIT_TLI        0

#define LSM9DS0_MFD_REG_TIME_LATENCY            0x3C
#define LSM9DS0_MFD_MASK_TIME_LATENCY_TLA       (BIT(7) | BIT(6) | BIT(5) | \
						 BIT(4) | BIT(3) | BIT(2) | \
						 BIT(1) | BIT(0))
#define LSM9DS0_MFD_SHIFT_TIME_LATENCY_TLA      0

#define LSM9DS0_MFD_REG_TIME_WINDOW             0x3D
#define LSM9DS0_MFD_MASK_TIME_WINDOW_TW         (BIT(7) | BIT(6) | BIT(5) | \
						 BIT(4) | BIT(3) | BIT(2) | \
						 BIT(1) | BIT(0))
#define LSM9DS0_MFD_SHIFT_TIME_WINDOW_TW        0

#define LSM9DS0_MFD_REG_ACT_THS                 0x3E
#define LSM9DS0_MFD_MASK_ACT_THS_ACTHS          (BIT(6) | BIT(5) | BIT(4) | \
						 BIT(3) | BIT(2) | BIT(1) | \
						 BIT(0))
#define LSM9DS0_MFD_SHIFT_ACT_THS_ACTHS         0

#define LSM9DS0_MFD_REG_ACT_DUR                 0x3F
#define LSM9DS0_MFD_MASK_ACT_DUR_ACTD           (BIT(7) | BIT(6) | BIT(5) | \
						 BIT(4) | BIT(3) | BIT(2) | \
						 BIT(1) | BIT(0))
#define LMS9DS0_MFD_SHIFT_ACT_DUR_ACTD          0

#if defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_0)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	0
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_3_125)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	1
	#define LSM9DS0_MFD_ACCEL_FORCE_MAX_MODR_50
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_6_25)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	2
	#define LSM9DS0_MFD_ACCEL_FORCE_MAX_MODR_50
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_12_5)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	3
	#define LSM9DS0_MFD_ACCEL_FORCE_MAX_MODR_50
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_25)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	4
	#define LSM9DS0_MFD_ACCEL_FORCE_MAX_MODR_50
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_50)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	5
	#define LSM9DS0_MFD_ACCEL_FORCE_MAX_MODR_50
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_100)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	6
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_200)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	7
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_400)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	8
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_800)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	9
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_1600)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_AODR	10
#endif

#if defined(CONFIG_LSM9DS0_MFD_ACCEL_FULL_SCALE_2)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_FS	0
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_FULL_SCALE_4)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_FS	1
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_FULL_SCALE_6)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_FS	2
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_FULL_SCALE_8)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_FS	3
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_FULL_SCALE_16)
	#define LSM9DS0_MFD_ACCEL_DEFAULT_FS	4
#endif

#if defined(CONFIG_LSM9DS0_MFD_ACCEL_ENABLE_X)
	#define LSM9DS0_MFD_ACCEL_ENABLE_X	1
#else
	#define LSM9DS0_MFD_ACCEL_ENABLE_X	0
#endif

#if defined(CONFIG_LSM9DS0_MFD_ACCEL_ENABLE_Y)
	#define LSM9DS0_MFD_ACCEL_ENABLE_Y	1
#else
	#define LSM9DS0_MFD_ACCEL_ENABLE_Y	0
#endif

#if defined(CONFIG_LSM9DS0_MFD_ACCEL_ENABLE_Z)
	#define LSM9DS0_MFD_ACCEL_ENABLE_Z	1
#else
	#define LSM9DS0_MFD_ACCEL_ENABLE_Z	0
#endif

#if defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_RUNTIME) || \
	defined(CONFIG_LSM9DS0_MFD_ACCEL_FULL_SCALE_RUNTIME)
	#define LSM9DS0_MFD_ATTR_SET_ACCEL
#endif

#if defined(CONFIG_LSM9DS0_MFD_MAGN_SAMPLING_RATE_3_125)
	#define LSM9DS0_MFD_MAGN_DEFAULT_M_ODR	0
#elif defined(CONFIG_LSM9DS0_MFD_MAGN_SAMPLING_RATE_6_25)
	#define LSM9DS0_MFD_MAGN_DEFAULT_M_ODR	1
#elif defined(CONFIG_LSM9DS0_MFD_MAGN_SAMPLING_RATE_12_5)
	#define LSM9DS0_MFD_MAGN_DEFAULT_M_ODR	2
#elif defined(CONFIG_LSM9DS0_MFD_MAGN_SAMPLING_RATE_25)
	#define LSM9DS0_MFD_MAGN_DEFAULT_M_ODR	3
#elif defined(CONFIG_LSM9DS0_MFD_MAGN_SAMPLING_RATE_50)
	#define LSM9DS0_MFD_MAGN_DEFAULT_M_ODR	4
#elif defined(CONFIG_LSM9DS0_MFD_MAGN_SAMPLING_RATE_100)
	#if defined(LSM9DS0_MFD_ACCEL_FORCE_MAX_MODR_50)
		#define LSM9DS0_MFD_MAGN_DEFAULT_M_ODR	4
	#else
		#define LSM9DS0_MFD_MAGN_DEFAULT_M_ODR	5
	#endif
#endif

#if defined(CONFIG_LSM9DS0_MFD_MAGN_FULL_SCALE_2)
	#define LSM9DS0_MFD_MAGN_DEFAULT_FS	0
#elif defined(CONFIG_LSM9DS0_MFD_MAGN_FULL_SCALE_4)
	#define LSM9DS0_MFD_MAGN_DEFAULT_FS	1
#elif defined(CONFIG_LSM9DS0_MFD_MAGN_FULL_SCALE_8)
	#define LSM9DS0_MFD_MAGN_DEFAULT_FS	2
#elif defined(CONFIG_LSM9DS0_MFD_MAGN_FULL_SCALE_12)
	#define LSM9DS0_MFD_MAGN_DEFAULT_FS	3
#endif

#if defined(CONFIG_LSM9DS0_MFD_MAGN_SAMPLING_RATE_RUNTIME) || \
	defined(CONFIG_LSM9DS0_MFD_MAGN_FULL_SCALE_RUNTIME)
	#define LSM9DS0_MFD_ATTR_SET_MAGN
#endif

#if !defined(CONFIG_LSM9DS0_MFD_ACCEL_ENABLE_X) && \
	!defined(CONFIG_LSM9DS0_MFD_ACCEL_ENABLE_Y) && \
	!defined(CONFIG_LSM9DS0_MFD_ACCEL_ENABLE_Z)
	#define LSM9DS0_MFD_ACCEL_DISABLED
#elif defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_0) && \
	!defined(CONFIG_LSM9DS0_MFD_ACCEL_SAMPLING_RATE_RUNTIME)
	#define LSM9DS0_MFD_ACCEL_DISABLED
#elif !defined(CONFIG_LSM9DS0_MFD_ACCEL_ENABLE)
	#define LSM9DS0_MFD_ACCEL_DISABLED
#endif

#if !defined(CONFIG_LSM9DS0_MFD_MAGN_ENABLE)
	#define LSM9DS0_MFD_MAGN_DISABLED
#endif

#if !defined(CONFIG_LSM9DS0_MFD_TEMP_ENABLE)
	#define LSM9DS0_MFD_TEMP_DISABLED
#endif

#if defined(LSM9DS0_MFD_ATTR_SET_ACCEL) && defined(LSM9DS0_MFD_ACCEL_DISABLED)
	#undef LSM9DS0_MFD_ATTR_SET_ACCEL
#endif

#if defined(LSM9DS0_MFD_ATTR_SET_MAGN) && defined(LSM9DS0_MFD_MAGN_DISABLED)
	#undef LSM9DS0_MFD_ATTR_SET_MAGN
#endif

#if defined(LSM9DS0_MFD_ATTR_SET_ACCEL) || defined(LSM9DS0_MFD_ATTR_SET_MAGN)
	#define LSM9DS0_MFD_ATTR_SET
#endif

struct lsm9ds0_mfd_config {
	struct i2c_dt_spec i2c;
};

struct lsm9ds0_mfd_data {
#if !defined(LSM9DS0_MFD_ACCEL_DISABLED)
	int sample_accel_x, sample_accel_y, sample_accel_z;
#endif

#if !defined(LSM9DS0_MFD_MAGN_DISABLED)
	int sample_magn_x, sample_magn_y, sample_magn_z;
#endif

#if !defined(LSM9DS0_MFD_TEMP_DISABLED)
	int sample_temp;
#endif

#if defined(CONFIG_LSM9DS0_MFD_ACCEL_FULL_SCALE_RUNTIME)
#if !defined(LSM9DS0_MFD_ACCEL_DISABLED)
	uint8_t accel_fs, sample_accel_fs;
#endif
#endif

#if defined(CONFIG_LSM9DS0_MFD_MAGN_FULL_SCALE_RUNTIME)
#if !defined(LSM9DS0_MFD_MAGN_DISABLED)
	uint8_t magn_fs, sample_magn_fs;
#endif
#endif
};

#endif /* ZEPHYR_DRIVERS_SENSOR_LSM9DS0_MFD_LSM9DS0_MFD_H_ */
