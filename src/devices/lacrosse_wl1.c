/** @file
    LaCrosse Technology View LTV-R1, LTV-R3 Rainfall Gauge, LTV-W1/W2 Wind Sensor.

    Copyright (C) 2020 Mike Bruski (AJ9X) <michael.bruski@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "decoder.h"

/**
LaCrosse Technology View LTV-R1, LTV-R3 Rainfall Gauge, LTV-W1/W2 Wind Sensor and TFA View Rainfall Gauge 30.3802.02


## LTV-W1 (also LTV-W2):

Full preamble is `aaaaaaaaaaaaaa d2aa2dd4`.

    ID:24h BATTLOW:1b STARTUP:1b ?:2b SEQ:3h ?:1b 8h8h8h WIND:12d 12h CRC:8h TRAILER 8h8h8h8h8h8h8h8h

*/

static int lacrosse_wl1_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    // printf("Hello!!");
    // full preamble (LTV-R1) is `fff00000 aaaaaaaa d2aa2dd4`
    // full preamble (LTV-R3, LTV-W1, TFA 30.3802.02) is `aaaaaaaaaaaaaa d2aa2dd4`
    uint8_t const preamble_pattern[] = {0xd2, 0xaa, 0x2d, 0xd4};

    uint8_t b[20];
    //uint8_t b_all[200];

    if (bitbuffer->num_rows > 1) {
        decoder_logf(decoder, 1, __func__, "Too many rows: %d", bitbuffer->num_rows);
        return DECODE_FAIL_SANITY;
    }
    int msg_len = bitbuffer->bits_per_row[0];
    //if (msg_len < 170) { // allows shorter preamble for LTV-R3 (>200) and TFA 30.3802.02 (on 868MHz around 177)
    if (msg_len < 156) { // allows shorter preamble for LTV-R3 (>200) and TFA 30.3802.02 (on 868MHz around 177)
        decoder_logf(decoder, 1, __func__, "Packet too short: %d bits", msg_len);
        return DECODE_ABORT_LENGTH;
    //} else if (msg_len > 272) {
    } else if (msg_len > 290) {
        decoder_logf(decoder, 1, __func__, "Packet too long: %d bits", msg_len);
        return DECODE_ABORT_LENGTH;
    } else {
        decoder_logf(decoder, 1, __func__, "packet length: %d", msg_len);
    }

    int offset = bitbuffer_search(bitbuffer, 0, 0,
            preamble_pattern, sizeof(preamble_pattern) * 8);

    if (offset >= msg_len) {
        decoder_log(decoder, 1, __func__, "Sync word not found");
        return DECODE_ABORT_EARLY;
    }

    offset += sizeof(preamble_pattern) * 8;
    bitbuffer_extract_bytes(bitbuffer, 0, offset, b, 20 * 8);

    int chk = crc8(b, 11, 0x31, 0x00);
    decoder_log_bitrow(decoder, 1, __func__, b, bitbuffer->bits_per_row[0] - offset, "");
    
    if (chk != 0){
        decoder_log(decoder, 1, __func__, "CRC failed!");
        return DECODE_FAIL_MIC;
    }

    if (b[0] != 0x50){
        decoder_log(decoder, 1, __func__, "Wrong device!");
        return DECODE_FAIL_MIC;
    }

    //decoder_log_bitrow(decoder, 1, __func__, b, bitbuffer->bits_per_row[0] - offset, "");
    //decoder_log_bitrow(decoder, 1, __func__, bitbuffer->bb[0], bitbuffer->bits_per_row[0], "");

    // Note that the rain zero value is 00aa00 with a known byte order of HH??LL.
    // We just prepend the middle byte and assume whitening. Let's hope we get feedback someday.
    int id        = (b[0] << 16) | (b[1] << 8) | b[2];
    int batt_low  = (b[3] & 0x80) >> 7;
    int seq       = (b[3] & 0x0e) >> 1;
    int raw_temp  = (b[5] & 0x0f) << 8 | b[6];
    // base and/or scale adjustments
    float temp_c = (raw_temp - 400) * 0.1f;
    int water_detected = ((b[4] & 0xf0) >> 4) > 0 ;

    /* clang-format off */
    data_t *data = data_make(
            "model",            "",                 DATA_STRING, "LaCrosse-WL1",
            "id",               "Sensor ID",        DATA_FORMAT, "%06x", DATA_INT, id,
            "battery_ok",       "Battery",          DATA_INT,    !batt_low,
            "seq",              "Sequence",         DATA_INT,    seq,
            "temperature_C",    "Temperature",      DATA_FORMAT, "%.1f C",  DATA_DOUBLE, temp_c,
            "detect_wet",       "Wet detected",   DATA_INT, water_detected,
            "mic",              "Integrity",        DATA_STRING, "CRC",
            NULL);
    /* clang-format on */

    decoder_output_data(decoder, data);
    return 1;
}

static char const *const output_fields[] = {
        "model",
        "id",
        "battery_ok",
        "seq",
        "flags",
        "temperature_C",
        "detect_wet",
        "mic",
        NULL,
};

// flex decoder m=FSK_PCM, s=104, l=104, r=9600
// flex decoder m=FSK_PCM, s=107, l=107, r=5900
// flex decoder m=FSK_PCM, s=116, l=116, r=20000
// 868.3 MHz flex decoder m=FSK_PCM, s=58, l=58, r=4000
r_device const lacrosse_wl1 = {
        .name        = "LaCrosse Technology View LTV-WL1 Water Leak & Temperature Sensor",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 104,
        .long_width  = 104,
        .reset_limit = 9600,
        .decode_fn   = &lacrosse_wl1_decode,
        .fields      = output_fields,
};
