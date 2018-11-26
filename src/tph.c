#include <stdlib.h>
#include <string.h>
#include "i2c.h"
#include "util.h"
#include "tph.h"
#include "bme680.h"

#define TPH_ADDRESS 0xec

static uint8_t _new_data = 0;

static struct bme680_dev _sensor;
struct bme680_field_data _field_data;
static uint16_t _meas_time = 0;

static int8_t _tph_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    uint8_t *payload = (uint8_t *)malloc(1 + len);
    payload[0] = reg_addr;
    memcpy(payload+1,data,len);
    int8_t result = I2C_Transmit(dev_id << 1, payload, 1 + len);
    free(payload);

    return result;
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

int8_t TPH_Initialize()
{
    _sensor.dev_id = BME680_I2C_ADDR_PRIMARY;
    _sensor.intf = BME680_I2C_INTF;
    _sensor.write = _tph_i2c_write;
    _sensor.read = _tph_i2c_read;
    _sensor.delay_ms = Util_DelayMs;
    _sensor.amb_temp = 20;

    int8_t result = bme680_init(&_sensor);

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

    bme680_get_profile_dur(&_meas_time, &_sensor);

    return result;
}

int8_t TPH_StartMeasurement()
{
    _new_data = 0;

    _sensor.power_mode = BME680_FORCED_MODE;
	return bme680_set_sensor_mode(&_sensor);
}

int8_t TPH_CheckForNewData()
{
    if (_new_data)
        return 1;

    int8_t result = bme680_get_sensor_data(&_field_data, &_sensor);
    if (result == BME680_OK)
    {
    	_new_data = 1;
    	return 1;
    }
    else if (result == BME680_W_NO_NEW_DATA)
    {
    	return 0;
    }
    else
    	return result;
}

uint8_t TPH_GetPressure(float *pressure, enum TPH_PressureUnits units)
{
    if (pressure == NULL || (!_new_data && TPH_CheckForNewData() <= 0))
    {
        return 0;
    }

    if (units == TPH_PRESSURE_HPA)
    	*pressure = _field_data.pressure / 100.0f;
    else if (units == TPH_PRESSURE_INHG)
    	*pressure = _field_data.pressure / 3386.38867f;

    return 1;
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

uint16_t TPH_GetMeasTimeMs()
{
	return _meas_time;
}
