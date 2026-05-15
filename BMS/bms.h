/**
 * @file bms.h
 * @brief 嘉佰达保护板通用协议库 (V12)
 * @note 支持RS485/RS232/UART，波特率9600，大端模式
 */

#ifndef _BMS_H
#define _BMS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 常量定义
 *============================================================================*/

/* 帧起始/结束标志 */
#define BMS_FRAME_START        0xDD
#define BMS_FRAME_STOP         0x77

/* 读写标志 */
#define BMS_READ_CMD           0xA5
#define BMS_WRITE_CMD          0x5A

/* 响应状态码 */
#define BMS_STATUS_SUCCESS     0x00
#define BMS_STATUS_CMD_INVALID 0x80
#define BMS_STATUS_OP_INVALID  0x81
#define BMS_STATUS_CHECK_ERR   0x82
#define BMS_STATUS_PWD_ERR     0x83
#define BMS_STATUS_PWD_CHG_FAIL 0x84

/* 命令码 */
#define BMS_CMD_BASIC_INFO     0x03
#define BMS_CMD_CELL_VOLTAGE   0x04
#define BMS_CMD_HW_VERSION     0x05
#define BMS_CMD_PROTECT_CNT    0xAA
#define BMS_CMD_CONTROL_MOS    0xFB
#define BMS_CMD_RW_PARAM       0xFA
#define BMS_CMD_CONTROL        0x0A
#define BMS_CMD_FACTORY_PWD    0x0B
#define BMS_CMD_CHIP_TYPE      0x00   // 读取芯片类型 (使用0x00命令, 数据为空)
#define BMS_CMD_HEATING        0xFC

/* 控制MOS的YY值 (MOS管类别) */
#define BMS_MOS_CHARGE_ON      0x01   // 打开充电MOS (实际XX对应bit)
#define BMS_MOS_CHARGE_OFF     0x00   // 关闭充电MOS
#define BMS_MOS_DISCHARGE_ON   0x01
#define BMS_MOS_DISCHARGE_OFF  0x00

/* 控制指令功能码 (0x0A指令的AABB) */
#define BMS_CTRL_RESET_CAP     0x0100
#define BMS_CTRL_CLEAR_RECORD  0x0200
#define BMS_CTRL_RESET_MCU     0x0300
#define BMS_CTRL_CLEAR_PROTECT 0x0400
#define BMS_CTRL_SLEEP         0x0500
#define BMS_CTRL_POWER_DOWN    0x0600
#define BMS_CTRL_AUTO_BALANCE  0x0700
#define BMS_CTRL_STORAGE_MODE  0x0800
#define BMS_CTRL_SOC20_ENABLE  0x0900
#define BMS_CTRL_SOC20_FORCE   0x0A00
#define BMS_CTRL_FORCE_START   0x0B00
#define BMS_CTRL_FORCE_HEAT    0x0C00

/* 工厂模式密码 (默认) */
#define BMS_FACTORY_PWD_DEFAULT 0x5678

/**
 * @brief 最大支持的电池串数
 */
#define BMS_MAX_CELLS         32

/*==============================================================================
 * 数据结构
 *============================================================================*/

/**
 * @brief 基本信息 (0x03指令响应)
 */
typedef struct {
    uint16_t total_voltage;      // 总电压，单位10mV
    int16_t  current;            // 电流，单位10mA (正充电，负放电)
    uint16_t remain_capacity;    // 剩余容量，单位10mAh
    uint16_t nominal_capacity;   // 标称容量，单位10mAh
    uint16_t cycle_count;        // 循环次数
    uint16_t prod_date;          // 生产日期 (需解析)
    uint32_t balance_low;        // 均衡低16位 (1~16串)
    uint32_t balance_high;       // 均衡高16位 (17~32串)
    uint16_t protect_status;     // 保护状态位
    uint8_t  sw_version;         // 软件版本 (0x10=1.0)
    uint8_t  rsoc;               // 剩余容量百分比 (%)
    uint8_t  fet_status;         // MOS状态 (bit0充电,bit1放电)
    uint8_t  cell_count;         // 电池串数
    uint8_t  temp_sensor_count;  // 温度探头数量
    int16_t  temperatures[8];    // 温度值，单位0.1℃ (实际数量由temp_sensor_count决定)
} BMS_BasicInfo_t;

/**
 * @brief 单体电压信息
 */
typedef struct {
    uint8_t  cell_count;                // 实际电池串数
    uint16_t voltages[BMS_MAX_CELLS];   // 单体电压，单位mV
} BMS_CellVoltage_t;

/**
 * @brief 保护次数统计 (0xAA指令)
 */
typedef struct {
    uint16_t short_protect_cnt;     // 短路保护次数
    uint16_t charge_overcurrent_cnt; // 充电过流次数
    uint16_t discharge_overcurrent_cnt; // 放电过流次数
    uint16_t overvoltage_cnt;        // 单体过压次数
    uint16_t sys_reset_cnt;          // 系统重启次数 (若支持)
} BMS_ProtectCount_t;

/**
 * @brief 参数读取时的通用格式
 */
typedef struct {
    uint16_t param_index;   // 参数序号
    uint16_t value;         // 参数值 (对于多寄存器读取可使用数组，此处简化)
} BMS_Parameter_t;

/*==============================================================================
 * 校验和函数
 *============================================================================*/

/**
 * @brief 计算协议校验和 (命令码+长度+数据内容 的累加和取反+1)
 * @param cmd   命令码
 * @param len   数据长度
 * @param data  数据内容指针 (可为NULL)
 * @return 校验和 (2字节，大端模式)
 */
uint16_t BMS_CalcChecksum(uint8_t cmd, uint8_t len, const uint8_t *data);

/*==============================================================================
 * 帧构建函数 (生成待发送的字节流)
 *============================================================================*/

/**
 * @brief 构建通用命令帧
 * @param rw_flag   读写标志 (BMS_READ_CMD 或 BMS_WRITE_CMD)
 * @param cmd       命令码
 * @param data      数据内容 (可为NULL)
 * @param data_len  数据长度
 * @param callback_id  回调ID (最长4字节，可为NULL)
 * @param cb_len       回调ID长度 (0~4)
 * @param out_buf      输出缓冲区 (至少64字节)
 * @param out_len      输出帧长度
 * @return true 成功，false 缓冲区不足
 */
bool BMS_BuildFrame(uint8_t rw_flag, uint8_t cmd,
                    const uint8_t *data, uint8_t data_len,
                    const uint8_t *callback_id, uint8_t cb_len,
                    uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 读取基本信息 (0x03) 命令帧
 * @param out_buf  输出缓冲区
 * @param out_len  输出长度
 */
void BMS_MakeReadBasicInfoCmd(uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 读取单体电压 (0x04) 命令帧
 */
void BMS_MakeReadCellVoltageCmd(uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 读取硬件版本 (0x05) 命令帧
 */
void BMS_MakeReadHardwareVersionCmd(uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 读取保护次数 (0xAA) 命令帧
 */
void BMS_MakeReadProtectCountCmd(uint8_t *out_buf, uint16_t *out_len);

/*
 * @brief 控制MOS管 (0xFB) 命令帧
 */
void BMS_MakeControlMoscCmd(bool on, uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 控制充电MOS (0xFB) 命令帧
 * @param on true=打开充电MOS, false=关闭
 */
void BMS_MakeControlChargeMosCmd(bool on, uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 控制放电MOS (0xFB) 命令帧
 * @param on true=打开放电MOS, false=关闭
 */
void BMS_MakeControlDischargeMosCmd(bool on, uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 读取参数 (0xFA读模式) 命令帧
 * @param start_index  起始参数序号 (2字节)
 * @param reg_count    寄存器数量 (1~95)
 */
void BMS_MakeReadParameterCmd(uint16_t start_index, uint8_t reg_count,
                              uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 写入参数 (0xFA写模式) 命令帧 (需先进入工厂模式)
 * @param start_index  起始参数序号
 * @param reg_count    寄存器数量
 * @param values       参数值数组 (每个2字节, 大端)
 */
void BMS_MakeWriteParameterCmd(uint16_t start_index, uint8_t reg_count,
                               const uint16_t *values,
                               uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 进入工厂模式 (0x00命令 + 密码)
 * @param password  密码 (默认0x5678)
 */
void BMS_MakeEnterFactoryModeCmd(uint16_t password,
                                 uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 退出工厂模式 (0x01命令 + 固定码0x2828)
 */
void BMS_MakeExitFactoryModeCmd(uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 发送控制指令 (0x0A)
 * @param func_code  功能码 (如 BMS_CTRL_RESET_CAP)
 */
void BMS_MakeControlCmd(uint16_t func_code,
                        uint8_t *out_buf, uint16_t *out_len);

/**
 * @brief 修改蓝牙密码 (0x0B指令)
 * @param old_pwd    旧密码 (6字节ASCII字符串，注意格式)
 * @param new_pwd    新密码 (6字节)
 * @note 本函数仅构建修改密码帧，需用户发送。
 *       密码格式示例: "765828" -> 字节序列 0x37,0x36,0x35,0x38,0x32,0x38
 */
void BMS_MakeChangeBluetoothPwdCmd(const uint8_t *old_pwd, const uint8_t *new_pwd,
                                   uint8_t *out_buf, uint16_t *out_len);

/*==============================================================================
 * 响应解析函数 (从BMS返回的字节流中提取数据)
 *============================================================================*/

/**
 * @brief 解析BMS响应帧 (校验起始、停止、校验和，提取状态和数据)
 * @param rx_buf     接收到的字节流
 * @param rx_len     接收长度
 * @param out_cmd    返回命令码
 * @param out_status 返回状态码 (0成功)
 * @param out_data   返回数据内容缓冲区
 * @param out_data_len 返回数据实际长度
 * @param out_cb_id  返回回调ID (可选)
 * @return true 帧有效, false 无效
 */
bool BMS_ParseResponse(const uint8_t *rx_buf, uint16_t rx_len,
                       uint8_t *out_cmd, uint8_t *out_status,
                       const uint8_t **out_data, uint8_t *out_data_len,
                       const uint8_t **out_cb_id, uint8_t *out_cb_len);

/**
 * @brief 解析基本信息响应
 * @param data     数据内容指针 (来自BMS_ParseResponse的out_data)
 * @param data_len 数据长度
 * @param info     输出结构体
 * @return true 成功
 */
bool BMS_ParseBasicInfo(const uint8_t *data, uint8_t data_len, BMS_BasicInfo_t *info);

/**
 * @brief 解析单体电压响应
 * @param data     数据内容
 * @param data_len 数据长度
 * @param cell_voltage 输出电压结构体
 * @return true 成功
 */
bool BMS_ParseCellVoltage(const uint8_t *data, uint8_t data_len,BMS_CellVoltage_t *cell_voltage);

/**
 * @brief 解析硬件版本字符串
 * @param data     数据内容 (第一个字节为长度N, 后续N个ASCII字符)
 * @param data_len 数据长度
 * @param out_str  输出字符串缓冲区 (至少32字节)
 * @param max_len  缓冲区大小
 * @return true 成功
 */
bool BMS_ParseHardwareVersion(const uint8_t *data, uint8_t data_len,
                              char *out_str, uint8_t max_len);

/**
 * @brief 解析保护次数统计
 * @param data     数据内容
 * @param data_len 数据长度 (22或24字节)
 * @param cnt      输出结构体
 * @return true 成功
 */
bool BMS_ParseProtectCount(const uint8_t *data, uint8_t data_len,
                           BMS_ProtectCount_t *cnt);

/**
 * @brief 解析参数读取响应 (提取寄存器值)
 * @param data     数据内容 (格式: 参数序号高字节, 低字节, 数据高字节, 低字节...)
 * @param data_len 数据长度
 * @param values   输出值数组 (每个2字节)
 * @param reg_cnt  返回寄存器数量
 * @return true 成功
 */
bool BMS_ParseParameterResponse(const uint8_t *data, uint8_t data_len,
                                uint16_t *values, uint8_t *reg_cnt);

#ifdef __cplusplus
}
#endif

#endif /* _BMS_H */