#include <string.h>
#include "i2c.h"
#include "tph.h"
#include "bme680.h"

#define TPH_ADDRESS 0xec

static uint8_t _new_data = 0;

static uint8_t _is_init = 0;
static struct bme680_dev _sensor;

static int8_t _tph_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    uint8_t *payload = (uint8_t *)malloc(1 + len);
    payload[0] = reg_addr;
    memcpy(payload+1,data,len);
    int8_t result = I2C_Transmit(dev_id << 1, payload, 1 + len);
    free(payload);
}

static int8_t _tph_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{

    uint8_t result = I2C_Transmit(dev_id << 1, &reg_addr, 1);
    if (result == 0)
    {
        result = I2C_Receive(dev_id << 1, data, len);
    }

    return (int8_t)result;
}

uint8_t TPH_Initialize()
{
    _sensor.dev_id = BME680_I2C_ADDR_PRIMARY;
    _sensor.intf = BME680_I2C_INTF;
    _sensor.write = _tph_i2c_write;
    _sensor.read = _tph_i2c_read;
    _sensor.delay_ms = NULL; // TODO: delay function mandatory
    _sensor.amb_temp = 20;

    int16_t result = bme680_init(&_sensor);

    if (result != BME680_OK)
        return result;

    _sensor.tph_sett.os_hum = BME680_OS_2X;
    _sensor.tph_sett.os_pres = BME680_OS_2X;
    _sensor.tph_sett.os_temp = BME680_OS_2X;
    _sensor.tph_sett.filter = BME680_FILTER_SIZE_3;
    _sensor.gas_sett.run_gas = BME680_DISABLE_GAS_MEAS;
    _sensor.power_mode = BME680_FORCED_MODE;

    uint16_t set_required_settings = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL;
    result = bme680_set_sensor_settings(set_required_settings,&_sensor)
        || bme680_set_sensor_mode(&_sensor);

    return result;
}

void TPH_StartMeasurement(uint8_t temp_oversampling, uint8_t pressure_oversampling, uint8_t humidity_oversampling)
{
    _new_data = 0;

    uint8_t payload[4] = {
        0x72, // ctrl_hum register
        humidity_oversampling,
        0x74, // ctrl_meas register
        (temp_oversampling << 5) | (pressure_oversampling << 2) | 0x01  // the 0x01 is the "mode" setting,
                                                                        // which starts a measurement
    };

    I2C_Transmit(TPH_ADDRESS, payload, 4);
}

uint8_t TPH_CheckForNewData()
{
    if (_new_data)
        return 1;

    uint8_t reg = 0x1d; // meas_status_0 register
    uint8_t rx_payload[1] = {0};

    I2C_Transmit(TPH_ADDRESS, &reg, 1);
    I2C_Receive(TPH_ADDRESS, rx_payload, 1);

    if (rx_payload[0] & 0x80)
    {
        _new_data = 1;
        return 1;
    }

    return 0;
}

uint8_t TPH_GetPressure(float *pressure)
{
    if (!_new_data && !TPH_CheckForNewData())
    {
        return 0;
    }

    uint8_t reg = 0x1f;
    uint8_t rx_payload[3] = {0};

    I2C_Transmit(TPH_ADDRESS, &reg, 1);
    I2C_Receive(TPH_ADDRESS, rx_payload, 3);

    uint32_t raw_pressure =
        rx_payload[0] << 12
        | rx_payload[1] << 4
        | rx_payload[2] >> 4;
}

uint8_t TPH_GetTemperature(float *temperature)
{
    if (!_new_data && !TPH_CheckForNewData())
    {
        return 0;
    }
}

uint8_t TPH_GetHumidity(float *humidity)
{
    if (!_new_data && !TPH_CheckForNewData())
    {
        return 0;
    }
}