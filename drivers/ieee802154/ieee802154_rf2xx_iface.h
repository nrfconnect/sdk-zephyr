/* ieee802154_rf2xx_iface.h - ATMEL RF2XX transceiver interface */

/*
 * Copyright (c) 2019 Gerson Fernando Budke
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_IEEE802154_IEEE802154_RF2XX_IFACE_H_
#define ZEPHYR_DRIVERS_IEEE802154_IEEE802154_RF2XX_IFACE_H_

/**
 * @brief Resets the TRX radio
 *
 * @param[in] dev   Transceiver device instance
 */
void rf2xx_iface_phy_rst(struct device *dev);

/**
 * @brief Start TX transmition
 *
 * @param[in] dev   Transceiver device instance
 */
void rf2xx_iface_phy_tx_start(struct device *dev);

/**
 * @brief Reads current value from a transceiver register
 *
 * This function reads the current value from a transceiver register.
 *
 * @param[in] dev   Transceiver device instance
 * @param[in] addr  Specifies the address of the trx register
 * from which the data shall be read
 *
 * @return value of the register read
 */
u8_t rf2xx_iface_reg_read(struct device *dev,
			  u8_t addr);

/**
 * @brief Writes data into a transceiver register
 *
 * This function writes a value into transceiver register.
 *
 * @param[in] dev   Transceiver device instance
 * @param[in] addr  Address of the trx register
 * @param[in] data  Data to be written to trx register
 *
 */
void rf2xx_iface_reg_write(struct device *dev,
			   u8_t addr,
			   u8_t data);

/**
 * @brief Subregister read
 *
 * @param[in] dev   Transceiver device instance
 * @param[in] addr  offset of the register
 * @param[in] mask  bit mask of the subregister
 * @param[in] pos   bit position of the subregister
 *
 * @return  value of the read bit(s)
 */
u8_t rf2xx_iface_bit_read(struct device *dev,
			  u8_t addr,
			  u8_t mask,
			  u8_t pos);

/**
 * @brief Subregister write
 *
 * @param[in]   dev       Transceiver device instance
 * @param[in]   reg_addr  Offset of the register
 * @param[in]   mask      Bit mask of the subregister
 * @param[in]   pos       Bit position of the subregister
 * @param[out]  new_value Data, which is muxed into the register
 */
void rf2xx_iface_bit_write(struct device *dev,
			   u8_t reg_addr,
			   u8_t mask,
			   u8_t pos,
			   u8_t new_value);

/**
 * @brief Reads frame buffer of the transceiver
 *
 * This function reads the frame buffer of the transceiver.
 *
 * @param[in]   dev     Transceiver device instance
 * @param[out]  data    Pointer to the location to store frame
 * @param[in]   length  Number of bytes to be read from the frame
 */
void rf2xx_iface_frame_read(struct device *dev,
			    u8_t *data,
			    u8_t length);

/**
 * @brief Writes data into frame buffer of the transceiver
 *
 * This function writes data into the frame buffer of the transceiver
 *
 * @param[in] dev    Transceiver device instance
 * @param[in] data   Pointer to data to be written into frame buffer
 * @param[in] length Number of bytes to be written into frame buffer
 */
void rf2xx_iface_frame_write(struct device *dev,
			     u8_t *data,
			     u8_t length);

#endif /* ZEPHYR_DRIVERS_IEEE802154_IEEE802154_RF2XX_IFACE_H_ */
