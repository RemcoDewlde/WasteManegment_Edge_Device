#include <Arduino.h>
#include "esp_adc_cal.h"

float readBattery()
{
    uint32_t value = 0;
    int rounds = 11;
    esp_adc_cal_characteristics_t adc_chars;

    // battery voltage divided by 2 can be measured at GPIO34, which equals ADC1_CHANNEL6
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

    // to avoid noise, sample the pin several times and average the result
    for (int i = 1; i <= rounds; i++)
    {
        value += adc1_get_raw(ADC1_CHANNEL_6);
    }
    value /= (uint32_t)rounds;

    // due to the voltage divider (1M+1M) values must be multiplied by 2
    // and convert mV to V
    return esp_adc_cal_raw_to_voltage(value, &adc_chars) * 2.0 / 1000.0;
}

float get_battery_percentage()
{
    float output = 0.0;             // output value
    const float battery_max = 4.20; // maximum voltage of battery
    const float battery_min = 3.0;
    output = ((readBattery() - battery_min) / (battery_max - battery_min)) * 100;
    return output;
}