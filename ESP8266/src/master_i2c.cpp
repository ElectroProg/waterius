#include "master_i2c.h"
#include "Logging.h"
#include <Wire.h>
#include <Arduino.h>
#include "setup.h"

//https://github.com/lammertb/libcrc/blob/600316a01924fd5bb9d47d535db5b8f3987db178/src/crc8.c


// https://gist.github.com/brimston3/83cdeda8f7d2cf55717b83f0d32f9b5e
// https://www.onlinegdb.com/online_c++_compiler
// Dallas CRC x8+x5+x4+1
uint8_t crc_8(unsigned char *b, size_t num_bytes, uint8_t crc) {
    uint8_t i;
    for (size_t a = 0; a < num_bytes; a++) {
        i = (*(b+a) ^ crc) & 0xff;
        crc = 0;
        if (i & 1)
            crc ^= 0x5e;
        if (i & 2)
            crc ^= 0xbc;
        if (i & 4)
            crc ^= 0x61;
        if (i & 8)
            crc ^= 0xc2;
        if (i & 0x10)
            crc ^= 0x9d;
        if (i & 0x20)
            crc ^= 0x23;
        if (i & 0x40)
            crc ^= 0x46;
        if (i & 0x80)
            crc ^= 0x8c;
    }
    return crc;
}



void MasterI2C::begin() {

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000L);
    Wire.setClockStretchLimit(2500L);  // Иначе связь с Attiny не надежная будут FF FF в хвосте посылки 
}


bool MasterI2C::sendCmd(const char cmd ) {

    Wire.beginTransmission( I2C_SLAVE_ADDR );
    if (Wire.write(cmd) != 1){
        LOG_ERROR(FPSTR(S_I2C), F("Write cmd failed"));
        return false;
    }
    int err = Wire.endTransmission(true);
    if (err != 0) {
        LOG_ERROR(FPSTR(S_I2C), "end error:" << err);
        return false;
    }    
    
    delay(1); // Дадим Attiny время подумать 
    return true;
}

bool MasterI2C::getByte(uint8_t &value, uint8_t &crc) {

    if (Wire.requestFrom( I2C_SLAVE_ADDR, 1 ) != 1) {
        LOG_ERROR(FPSTR(S_I2C), F("RequestFrom failed"));
        return false;
    }
    value = Wire.read();
    crc = crc_8(&value, 1, crc);
    return true;
}

bool MasterI2C::getUint16(uint16_t &value, uint8_t &crc) {

    uint8_t i1, i2;
    if (getByte(i1, crc) && getByte(i2, crc)) {
        value = i2;
        value = value << 8;
        value |= i1;
        return true;
    }
    return false;
}

bool MasterI2C::getUint(uint32_t &value, uint8_t &crc) {

    uint8_t i1, i2, i3, i4;
    if (getByte(i1, crc) && getByte(i2, crc) && getByte(i3, crc) && getByte(i4, crc)) {
        value = i4;
        value = value << 8;
        value |= i3;
        value = value << 8;
        value |= i2;
        value = value << 8;
        value |= i1;
        //вот так не работает из-за преобразования типов:
        //value = i1 | (i2 << 8) | (i3 << 16) | (i4 << 24);
        return true;
    }
    return false;
}

bool MasterI2C::getMode(uint8_t &mode) {

    uint8_t crc; //not used
    mode = TRANSMIT_MODE;
    if (!sendCmd('M') || !getByte(mode, crc)) {
        LOG_ERROR(FPSTR(S_I2C), F("GetMode failed. Check i2c line."));
        return false;
    }
    LOG_INFO(FPSTR(S_I2C), "mode: " << mode);
    return true;
}

bool MasterI2C::getSlaveData(SlaveData &data) {

    sendCmd('B');

    data.diagnostic = WATERIUS_NO_LINK;

    uint8_t dummy, crc = 0;
    bool good = getByte(data.version, crc);
    good &= getByte(data.service, crc);
    good &= getUint(data.voltage, crc);

    good &= getByte(data.resets, crc);
    good &= getByte(data.model, crc);
    good &= getByte(data.state0, crc);
    good &= getByte(data.state1, crc);
    if (data.model == WATERIUS_4C2W) {
        good &= getByte(data.state2, crc);
        good &= getByte(data.state3, crc);
    }

    good &= getUint(data.impulses0, crc);
    good &= getUint(data.impulses1, crc);
    if (data.model == WATERIUS_4C2W) {
        good &= getUint(data.impulses2, crc);
        good &= getUint(data.impulses3, crc);
    }

    good &= getUint16(data.adc0, crc);
    good &= getUint16(data.adc1, crc);
    if (data.model == WATERIUS_4C2W) {
        good &= getUint16(data.adc2, crc);
        good &= getUint16(data.adc3, crc);
    }

    good &= getByte(data.crc, dummy);

    if (good) {
        data.diagnostic = (data.crc == crc) ? WATERIUS_OK : WATERIUS_BAD_CRC;
    }

    switch (data.diagnostic) {
        case WATERIUS_BAD_CRC:
            LOG_ERROR(FPSTR(S_I2C), F("CRC wrong"));
        case WATERIUS_OK:
            LOG_INFO(FPSTR(S_I2C), F("version: ") << data.version);
            LOG_INFO(FPSTR(S_I2C), F("service: ") << data.service);
            LOG_INFO(FPSTR(S_I2C), F("voltage: ") << data.voltage);
            LOG_INFO(FPSTR(S_I2C), F("resets: ") << data.resets);
            LOG_INFO(FPSTR(S_I2C), F("MODEL: ") << data.model);
            LOG_INFO(FPSTR(S_I2C), F("state0: ") << data.state0);
            LOG_INFO(FPSTR(S_I2C), F("state1: ") << data.state1);
            LOG_INFO(FPSTR(S_I2C), F("state2: ") << data.state2);
            LOG_INFO(FPSTR(S_I2C), F("state3: ") << data.state3);
            LOG_INFO(FPSTR(S_I2C), F("impulses0: ") << data.impulses0);
            LOG_INFO(FPSTR(S_I2C), F("impulses1: ") << data.impulses1);
            LOG_INFO(FPSTR(S_I2C), F("impulses2: ") << data.impulses2);
            LOG_INFO(FPSTR(S_I2C), F("impulses3: ") << data.impulses3);
            LOG_INFO(FPSTR(S_I2C), F("adc0: ") << data.adc0);
            LOG_INFO(FPSTR(S_I2C), F("adc1: ") << data.adc1);
            LOG_INFO(FPSTR(S_I2C), F("adc2: ") << data.adc2);
            LOG_INFO(FPSTR(S_I2C), F("adc3: ") << data.adc3);
            LOG_INFO(FPSTR(S_I2C), F("CRC ok"));
        break;
        case WATERIUS_NO_LINK:
            LOG_ERROR(FPSTR(S_I2C), F("Data failed"));
    };

    return data.diagnostic == WATERIUS_OK;
}


