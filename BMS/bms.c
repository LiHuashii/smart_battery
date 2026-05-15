/**
 * @file bms.c
 * @brief 嘉佰达保护板通用协议实现
 */

#include "bms.h"
#include <string.h>

/* 辅助宏：大端读取16位 */
#define READ_BE16(p) ((uint16_t)((p)[0] << 8) | (p)[1])
#define WRITE_BE16(p, v) do { (p)[0] = (uint8_t)((v) >> 8); (p)[1] = (uint8_t)(v); } while(0)

/* 计算校验和（命令码+长度+数据） */
uint16_t BMS_CalcChecksum(uint8_t cmd, uint8_t len, const uint8_t *data)
{
    uint16_t sum = cmd + len;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum + 1;   // 取反加1
}

/**
 * @brief 计算BMS响应帧的校验和 (状态码+长度+数据)
 * @param status 状态码
 * @param len    数据长度
 * @param data   数据内容指针 (可为NULL)
 * @return 校验和 (2字节，大端模式)
 */
static uint16_t BMS_CalcRespChecksum(uint8_t status, uint8_t len, const uint8_t *data)
{
    uint16_t sum = status + len;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum + 1;
}

/* 构建通用命令帧 */
bool BMS_BuildFrame(uint8_t rw_flag, uint8_t cmd,
                    const uint8_t *data, uint8_t data_len,
                    const uint8_t *callback_id, uint8_t cb_len,
                    uint8_t *out_buf, uint16_t *out_len)
{
    uint16_t idx = 0;
    if (cb_len > 4) return false;

    out_buf[idx++] = BMS_FRAME_START;
    out_buf[idx++] = rw_flag;
    out_buf[idx++] = cmd;
    out_buf[idx++] = data_len;

    if (data_len && data) {
        memcpy(&out_buf[idx], data, data_len);
        idx += data_len;
    }

    uint16_t checksum = BMS_CalcChecksum(cmd, data_len, data);
    out_buf[idx++] = (uint8_t)(checksum >> 8);
    out_buf[idx++] = (uint8_t)(checksum & 0xFF);

    out_buf[idx++] = BMS_FRAME_STOP;

    if (cb_len && callback_id) {
        memcpy(&out_buf[idx], callback_id, cb_len);
        idx += cb_len;
    }

    *out_len = idx;
    return true;
}

/* 构建基本命令的辅助函数（无回调ID） */
static void build_simple_cmd(uint8_t cmd, uint8_t *buf, uint16_t *len)
{
    BMS_BuildFrame(BMS_READ_CMD, cmd, NULL, 0, NULL, 0, buf, len);
}

void BMS_MakeReadBasicInfoCmd(uint8_t *out_buf, uint16_t *out_len)
{
    build_simple_cmd(BMS_CMD_BASIC_INFO, out_buf, out_len);
}

void BMS_MakeReadCellVoltageCmd(uint8_t *out_buf, uint16_t *out_len)
{
    build_simple_cmd(BMS_CMD_CELL_VOLTAGE, out_buf, out_len);
}

void BMS_MakeReadHardwareVersionCmd(uint8_t *out_buf, uint16_t *out_len)
{
    build_simple_cmd(BMS_CMD_HW_VERSION, out_buf, out_len);
}

void BMS_MakeReadProtectCountCmd(uint8_t *out_buf, uint16_t *out_len)
{
    build_simple_cmd(BMS_CMD_PROTECT_CNT, out_buf, out_len);
}

void BMS_MakeControlMoscCmd(bool on, uint8_t *out_buf, uint16_t *out_len){
    uint8_t data[2] = {0x0A, (on ? 0x00 : 0x01)}; 
    BMS_BuildFrame(BMS_WRITE_CMD, BMS_CMD_CONTROL_MOS, data, 2, NULL, 0, out_buf, out_len);
}

// 控制充电 MOS：on=true 打开，false 关闭
void BMS_MakeControlChargeMosCmd(bool on, uint8_t *out_buf, uint16_t *out_len)
{
    uint8_t data[2] = {0x01, (on ? 0x00 : 0x01)};
    BMS_BuildFrame(BMS_WRITE_CMD, BMS_CMD_CONTROL_MOS, data, 2, NULL, 0, out_buf, out_len);
}

// 控制放电 MOS
void BMS_MakeControlDischargeMosCmd(bool on, uint8_t *out_buf, uint16_t *out_len)
{
    uint8_t data[2] = {0x00, (on ? 0x00 : 0x01)};
    BMS_BuildFrame(BMS_WRITE_CMD, BMS_CMD_CONTROL_MOS, data, 2, NULL, 0, out_buf, out_len);
}

void BMS_MakeReadParameterCmd(uint16_t start_index, uint8_t reg_count,
                              uint8_t *out_buf, uint16_t *out_len)
{
    uint8_t data[3];
    WRITE_BE16(&data[0], start_index);
    data[2] = reg_count;
    BMS_BuildFrame(BMS_READ_CMD, BMS_CMD_RW_PARAM, data, 3, NULL, 0, out_buf, out_len);
}

void BMS_MakeWriteParameterCmd(uint16_t start_index, uint8_t reg_count,
                               const uint16_t *values,
                               uint8_t *out_buf, uint16_t *out_len)
{
    uint8_t data[3 + reg_count * 2];
    WRITE_BE16(&data[0], start_index);
    data[2] = reg_count;
    for (uint8_t i = 0; i < reg_count; i++) {
        WRITE_BE16(&data[3 + i*2], values[i]);
    }
    BMS_BuildFrame(BMS_WRITE_CMD, BMS_CMD_RW_PARAM, data, 3 + reg_count*2, NULL, 0, out_buf, out_len);
}

void BMS_MakeEnterFactoryModeCmd(uint16_t password, uint8_t *out_buf, uint16_t *out_len)
{
    uint8_t data[2];
    WRITE_BE16(data, password);
    BMS_BuildFrame(BMS_WRITE_CMD, 0x00, data, 2, NULL, 0, out_buf, out_len);
}

void BMS_MakeExitFactoryModeCmd(uint8_t *out_buf, uint16_t *out_len)
{
    uint8_t data[2] = {0x28, 0x28};
    BMS_BuildFrame(BMS_WRITE_CMD, 0x01, data, 2, NULL, 0, out_buf, out_len);
}

void BMS_MakeControlCmd(uint16_t func_code, uint8_t *out_buf, uint16_t *out_len)
{
    uint8_t data[2];
    WRITE_BE16(data, func_code);
    BMS_BuildFrame(BMS_WRITE_CMD, BMS_CMD_CONTROL, data, 2, NULL, 0, out_buf, out_len);
}

void BMS_MakeChangeBluetoothPwdCmd(const uint8_t *old_pwd, const uint8_t *new_pwd,
                                   uint8_t *out_buf, uint16_t *out_len)
{
    uint8_t data[12];
    memcpy(data, old_pwd, 6);
    memcpy(data+6, new_pwd, 6);
    BMS_BuildFrame(BMS_WRITE_CMD, 0x07, data, 12, NULL, 0, out_buf, out_len);
}

/* 解析响应帧 */
bool BMS_ParseResponse(const uint8_t *rx_buf, uint16_t rx_len,
                       uint8_t *out_cmd, uint8_t *out_status,
                       const uint8_t **out_data, uint8_t *out_data_len,
                       const uint8_t **out_cb_id, uint8_t *out_cb_len)
{
    if (rx_len < 6) return false;
    if (rx_buf[0] != BMS_FRAME_START) return false;

    uint8_t cmd    = rx_buf[1];
    uint8_t status = rx_buf[2];
    uint8_t data_len = rx_buf[3];

    uint16_t expected_len = 4 + data_len + 2 + 1; // cmd+status+len + data + checksum2 + stop
    if (rx_len < expected_len) return false;

    // 正确的响应校验和计算（基于状态码）
    uint16_t calc_sum = BMS_CalcRespChecksum(status, data_len, (data_len ? &rx_buf[4] : NULL));
    uint16_t recv_sum = READ_BE16(&rx_buf[4 + data_len]);
    if (calc_sum != recv_sum) return false;

    if (rx_buf[4 + data_len + 2] != BMS_FRAME_STOP) return false;

    if (out_cmd) *out_cmd = cmd;
    if (out_status) *out_status = status;
    if (out_data) *out_data = (data_len ? &rx_buf[4] : NULL);
    if (out_data_len) *out_data_len = data_len;

    if (expected_len < rx_len) {
        uint8_t cb_len_actual = (uint8_t)(rx_len - expected_len);
        if (cb_len_actual > 4) cb_len_actual = 4;
        if (out_cb_id) *out_cb_id = &rx_buf[expected_len];
        if (out_cb_len) *out_cb_len = cb_len_actual;
    } else {
        if (out_cb_id) *out_cb_id = NULL;
        if (out_cb_len) *out_cb_len = 0;
    }
    return true;
}

/* 解析基本信息 */
bool BMS_ParseBasicInfo(const uint8_t *data, uint8_t data_len, BMS_BasicInfo_t *info)
{
    if (!data || data_len < 29) return false;  // 至少需要29字节（参考文档P4）

    info->total_voltage    = READ_BE16(&data[0]);
    info->current          = (int16_t)READ_BE16(&data[2]);
    info->remain_capacity  = READ_BE16(&data[4]);
    info->nominal_capacity = READ_BE16(&data[6]);
    info->cycle_count      = READ_BE16(&data[8]);
    info->prod_date        = READ_BE16(&data[10]);
    info->balance_low      = READ_BE16(&data[12]);
    info->balance_high     = READ_BE16(&data[14]);
    info->protect_status   = READ_BE16(&data[16]);
    info->sw_version       = data[18];
    info->rsoc             = data[19];
    info->fet_status       = data[20];
    info->cell_count       = data[21];
    info->temp_sensor_count = data[22];

    for (uint8_t i = 0; i < info->temp_sensor_count && i < 8; i++) {
        info->temperatures[i] = (int16_t)READ_BE16(&data[23 + i*2]);
    }
    return true;
}

/* 解析单体电压 */
bool BMS_ParseCellVoltage(const uint8_t *data, uint8_t data_len,BMS_CellVoltage_t *cell_voltage)
{
    if (!data || data_len < 2) return false;
    uint8_t count = data_len / 2;
    if (count > BMS_MAX_CELLS) count = BMS_MAX_CELLS;
    for (uint8_t i = 0; i < count; i++) {
        cell_voltage->voltages[i] = READ_BE16(&data[i*2]);
    }
    cell_voltage->cell_count = count;
    return true;
}

/* 解析硬件版本 */
bool BMS_ParseHardwareVersion(const uint8_t *data, uint8_t data_len,
                              char *out_str, uint8_t max_len)
{
    if (!data || data_len < 1) return false;
    uint8_t str_len = data[0];
    if (str_len > data_len - 1) str_len = data_len - 1;
    if (str_len > max_len - 1) str_len = max_len - 1;
    memcpy(out_str, &data[1], str_len);
    out_str[str_len] = '\0';
    return true;
}

/* 解析保护次数 */
bool BMS_ParseProtectCount(const uint8_t *data, uint8_t data_len,
                           BMS_ProtectCount_t *cnt)
{
    if (!data || (data_len != 22 && data_len != 24)) return false;
    cnt->short_protect_cnt         = READ_BE16(&data[0]);
    cnt->charge_overcurrent_cnt    = READ_BE16(&data[2]);
    cnt->discharge_overcurrent_cnt = READ_BE16(&data[4]);
    cnt->overvoltage_cnt           = READ_BE16(&data[6]);
    if (data_len >= 24) {
        cnt->sys_reset_cnt = READ_BE16(&data[22]);
    } else {
        cnt->sys_reset_cnt = 0;
    }
    return true;
}

/* 解析参数读取响应 */
bool BMS_ParseParameterResponse(const uint8_t *data, uint8_t data_len,
                                uint16_t *values, uint8_t *reg_cnt)
{
    if (!data || data_len < 3) return false;
    // 数据格式: param_index(2B) + 后面每2字节一个值
    uint8_t count = (data_len - 2) / 2;
    for (uint8_t i = 0; i < count; i++) {
        values[i] = READ_BE16(&data[2 + i*2]);
    }
    if (reg_cnt) *reg_cnt = count;
    return true;
}
