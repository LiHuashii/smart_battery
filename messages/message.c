#include "message.h"
#include <string.h>
#include "fh_log.h"
#include "cmsis_os.h"
#include "usart.h"
#include "fdcan.h"

BMS_BasicInfo_t bms_basicinfo;
BMS_CellVoltage_t bms_cellvoltage;

/**
 * @brief 处理接收到的完整BMS帧
 * @param buffer  接收缓冲区（包含从0xDD到0x77的完整帧）
 * @param length  缓冲区有效数据长度
 */
static void ProcessBMSFrame(const uint8_t *buffer, uint16_t length)
{
    uint8_t cmd, status;
    const uint8_t *data;
    uint8_t data_len;
    const uint8_t *cb_id;
    uint8_t cb_len;

    // 1. 解析帧，校验校验和和帧格式
    bool valid = BMS_ParseResponse(buffer, length,
                                   &cmd, &status,
                                   &data, &data_len,
                                   &cb_id, &cb_len);
    if (!valid) {
        // 帧无效：可能是校验和错误、长度不对等
        LOGE("BMS frame invalid or checksum error!");
        return;
    }

    // 2. 检查状态码（非0表示错误）
    if (status != BMS_STATUS_SUCCESS) {
        LOGE("BMS response error, status=0x%02X", status);
        switch (status) {
            case BMS_STATUS_CMD_INVALID:
                LOGE("Command not supported");
                break;
            case BMS_STATUS_OP_INVALID:
                LOGE("Invalid operation (need factory mode?)");
                break;
            case BMS_STATUS_PWD_ERR:
                LOGE("Password error");
                break;
            default:
                LOGE("Unknown error");
        }
        return;
    }

    // 3. 根据命令码解析数据
    switch (cmd) {
        case BMS_CMD_BASIC_INFO:   // 0x03
        {
            if (BMS_ParseBasicInfo(data, data_len, &bms_basicinfo)) {
                LOGI("=== Basic Info ===");
                LOGI("Total voltage: %d.%02dV", bms_basicinfo.total_voltage/100, bms_basicinfo.total_voltage%100);
                LOGI("Current: %d.%02dA", bms_basicinfo.current/100, (bms_basicinfo.current>0?bms_basicinfo.current%100:-bms_basicinfo.current%100));
                LOGI("Remain capacity: %dmAh", bms_basicinfo.remain_capacity * 10);
                LOGI("Nominal capacity: %dmAh", bms_basicinfo.nominal_capacity * 10);
                LOGI("Cycle count: %d", bms_basicinfo.cycle_count);
                LOGI("RSOC: %d%%", bms_basicinfo.rsoc);
                LOGI("FET: Charge=%s, Discharge=%s",
                       (bms_basicinfo.fet_status & 0x01) ? "ON" : "OFF",
                       (bms_basicinfo.fet_status & 0x02) ? "ON" : "OFF");
                LOGI("Cell count: %d", bms_basicinfo.cell_count);
                LOGI("Temp sensors: %d", bms_basicinfo.temp_sensor_count);
                for (int i=0; i<bms_basicinfo.temp_sensor_count && i<8; i++) {
                    // 温度单位0.1℃，开尔文 -> 摄氏度
                    int16_t temp_c = bms_basicinfo.temperatures[i] - 2731;
                    LOGI("Temp%d: %d.%d℃", i+1, temp_c/10, temp_c%10);
                }
            }
            break;
        }

        case BMS_CMD_CELL_VOLTAGE:   // 0x04
        {
            if (BMS_ParseCellVoltage(data, data_len, &bms_cellvoltage)) {
                LOGI("=== Cell Voltages (%d cells) ===", bms_cellvoltage.cell_count);
                for (uint8_t i=0; i<bms_cellvoltage.cell_count; i++) {
                    LOGI("Cell%2d: %d.%03dV", i+1, bms_cellvoltage.voltages[i]/1000, bms_cellvoltage.voltages[i]%1000);
                }
            }
            break;
        }

        case BMS_CMD_HW_VERSION:     // 0x05
        {
            char version[32];
            if (BMS_ParseHardwareVersion(data, data_len, version, sizeof(version))) {
                LOGI("Hardware version: %s", version);
            }
            break;
        }

        case BMS_CMD_PROTECT_CNT:    // 0xAA
        {
            BMS_ProtectCount_t cnt;
            if (BMS_ParseProtectCount(data, data_len, &cnt)) {
                LOGI("=== Protection Count ===");
                LOGI("Short circuit:   %d", cnt.short_protect_cnt);
                LOGI("Charge overcurrent: %d", cnt.charge_overcurrent_cnt);
                LOGI("Discharge overcurrent: %d", cnt.discharge_overcurrent_cnt);
                LOGI("Overvoltage:     %d", cnt.overvoltage_cnt);
                if (data_len >= 24) {
                    LOGI("System reset:    %d", cnt.sys_reset_cnt);
                }
            }
            break;
        }

        case BMS_CMD_RW_PARAM:       // 0xFA (参数读取响应)
        {
            uint16_t values[32];
            uint8_t reg_cnt;
            if (BMS_ParseParameterResponse(data, data_len, values, &reg_cnt)) {
                LOGI("=== Parameter Read (%d registers) ===", reg_cnt);
                for (uint8_t i=0; i<reg_cnt; i++) {
                    LOGI("Reg%d: 0x%04X (%d)", i, values[i], values[i]);
                }
            }
            break;
        }

        case BMS_CMD_CONTROL_MOS:    // 0xFB (写入MOS回复)
            LOGI("MOS control command executed.");
            break;

        case BMS_CMD_CONTROL:        // 0x0A (控制指令回复)
            LOGI("Control command executed.");
            break;

        case 0x00:   // 读取芯片类型或进入工厂模式的回复
            // 注意：进入工厂模式回复也是0x00命令码，数据长度为0
            if (data_len == 0) {
                LOGI("Factory mode entered / Chip type read.");
            }
            break;

        case 0x01:   // 退出工厂模式回复
            LOGI("Factory mode exited.");
            break;

        default:
            LOGI("Unknown command: 0x%02X", cmd);
            break;
    }

    // 如果存在回调ID，可以打印出来（基本用不到）
    if (cb_len > 0 && cb_id) {
        LOGI("Callback ID: ");
        for (uint8_t i=0; i<cb_len; i++) {
            LOGI("%02X ", cb_id[i]);
        }
        LOGI("\n");
    }
}

uint8_t bms_rx_buffer[BMS_BUFFSIZE];
uint16_t bms_rx_len;
extern osSemaphoreId_t BMS_RXHandle;

void USART2_IQR(void){
	if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET){
		__HAL_UART_CLEAR_IDLEFLAG(&huart2);	// 清除空闲中断标志位
		HAL_UART_DMAStop(&huart2);	// 停止DMA传输
		bms_rx_len = BMS_BUFFSIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx) - 1;	// 接收的数据长度 = DMA接收通道总长度 - 剩余长度
        ProcessBMSFrame(bms_rx_buffer, bms_rx_len);
		osSemaphoreRelease(BMS_RXHandle);//释放信号量 完成一次接收
		HAL_UART_Receive_DMA(&huart2, bms_rx_buffer, sizeof(bms_rx_buffer)-1);//开启DMA接收
	}
}

void BMS_SendData(uint8_t *data, uint16_t len) {
    HAL_UART_Transmit_DMA(&huart2, data, len); 
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0Index)
{
    FDCAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[64];
    if (HAL_FDCAN_GetRxMessage(hfdcan, RxFifo0Index, &RxHeader, RxData) == HAL_OK)
    {
        // 处理接收到的数据
        if (RxHeader.Identifier == 0)   // 过滤器中已限制，但可做二次确认
        {
            // 用户处理代码
        }
    }
}

uint32_t CAN_ID = 0x199A946A;
/**
  * @brief  FDCAN1 发送数据（扩展帧，支持 CAN FD）
  * @param  pData: 待发送数据缓冲区指针
  * @param  len:   数据长度（字节，最大 64）
  * @retval HAL 状态：HAL_OK / HAL_ERROR / HAL_BUSY / HAL_TIMEOUT
  */
HAL_StatusTypeDef FDCAN1_SendData(uint8_t *pData)
{
    FDCAN_TxHeaderTypeDef TxHeader;
    uint8_t txData[64];  // 最大支持 64 字节
    uint32_t txFifoFreeLevel;

    /* 1. 参数检查 */
    if (pData == NULL)
    {
        return HAL_ERROR;
    }

    /* 2. 检查发送 FIFO 是否有空余（至少 1 个空位）*/
    txFifoFreeLevel = HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1);
    if (txFifoFreeLevel == 0)
    {
        return HAL_BUSY;   // 发送队列满，可增加等待重试逻辑
    }

    /* 3. 配置发送头 */
    TxHeader.Identifier = CAN_ID;                 // 29 位扩展 ID
    TxHeader.IdType = FDCAN_EXTENDED_ID;          // 扩展帧
    TxHeader.TxFrameType = FDCAN_DATA_FRAME;      // 数据帧（非远程帧）
    TxHeader.DataLength = FDCAN_DLC_BYTES_64;     // 实际字节长度（HAL 会自动转换为 DLC）
    TxHeader.ErrorStateIndicator = DISABLE;       // 不设置错误状态指示
    TxHeader.BitRateSwitch = ENABLE;              // 使能 BRS（因为初始化中启用了 FDCAN_FRAME_FD_BRS）
    TxHeader.FDFormat = FDCAN_FD_CAN;             // CAN FD 格式
    TxHeader.TxEventFifoControl = DISABLE;        // 不将发送事件存储到 Tx Event FIFO
    TxHeader.MessageMarker = 0;                   // 可选的消息标记

    /* 4. 复制数据到发送缓冲区 */
    memcpy(txData, pData, 64);

    /* 5. 将消息加入发送 FIFO */
    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, txData);
           
}

/**
 * 将 BMS 基础信息与单体电压打包到缓冲区（小端序，无填充字节）
 * @param buffer 输出缓冲区（需足够大：基础信息定长部分 + 温度个数*2 + 1字节cell_count + 电压个数*2）
 * @param basic  基础信息结构体指针
 * @param cell   单体电压结构体指针
 * @return       打包后的总字节数
 */
uint16_t pack_bms_data(uint8_t* buffer, const BMS_BasicInfo_t* basic, const BMS_CellVoltage_t* cell) {
    uint16_t offset = 0;
    uint8_t i;

    // ----- 基础信息（定长字段，共 2+2+2+2+2+2+4+4+2+1+1+1+1+1 = 27 字节 -----
    // 不计 temperatures[8]（动态处理），也不计最后两个字节（cell_count和temp_sensor_count已包含）
    // 实际上面列表有 total_voltage(2) current(2) remain_capacity(2) nominal_capacity(2) cycle_count(2) prod_date(2)
    // balance_low(4) balance_high(4) protect_status(2) sw_version(1) rsoc(1) fet_status(1) cell_count(1) temp_sensor_count(1)
    // 以上共 2*6 + 4*2 + 2 + 1*5 = 12+8+2+5 = 27 字节，然后再加上 temperatures 动态部分

    uint16_t temp;
    uint32_t temp32;

    // total_voltage
    temp = basic->total_voltage;
    buffer[offset++] = temp & 0xFF;
    buffer[offset++] = (temp >> 8) & 0xFF;

    // current (int16_t)
    temp = (uint16_t)basic->current;
    buffer[offset++] = temp & 0xFF;
    buffer[offset++] = (temp >> 8) & 0xFF;

    // remain_capacity
    temp = basic->remain_capacity;
    buffer[offset++] = temp & 0xFF;
    buffer[offset++] = (temp >> 8) & 0xFF;

    // nominal_capacity
    temp = basic->nominal_capacity;
    buffer[offset++] = temp & 0xFF;
    buffer[offset++] = (temp >> 8) & 0xFF;

    // cycle_count
    temp = basic->cycle_count;
    buffer[offset++] = temp & 0xFF;
    buffer[offset++] = (temp >> 8) & 0xFF;

    // prod_date
    temp = basic->prod_date;
    buffer[offset++] = temp & 0xFF;
    buffer[offset++] = (temp >> 8) & 0xFF;

    // balance_low
    temp32 = basic->balance_low;
    buffer[offset++] = temp32 & 0xFF;
    buffer[offset++] = (temp32 >> 8) & 0xFF;
    buffer[offset++] = (temp32 >> 16) & 0xFF;
    buffer[offset++] = (temp32 >> 24) & 0xFF;

    // balance_high
    temp32 = basic->balance_high;
    buffer[offset++] = temp32 & 0xFF;
    buffer[offset++] = (temp32 >> 8) & 0xFF;
    buffer[offset++] = (temp32 >> 16) & 0xFF;
    buffer[offset++] = (temp32 >> 24) & 0xFF;

    // protect_status
    temp = basic->protect_status;
    buffer[offset++] = temp & 0xFF;
    buffer[offset++] = (temp >> 8) & 0xFF;

    // sw_version
    buffer[offset++] = basic->sw_version;

    // rsoc
    buffer[offset++] = basic->rsoc;

    // fet_status
    buffer[offset++] = basic->fet_status;

    // cell_count (基础信息中的电池串数，保留)
    buffer[offset++] = basic->cell_count;

    // temp_sensor_count
    buffer[offset++] = basic->temp_sensor_count;

    // ----- 温度值（只打包前 temp_sensor_count 个，每个 int16_t 小端序）-----
    for (i = 0; i < basic->temp_sensor_count && i < 8; i++) {
        temp = (uint16_t)basic->temperatures[i];
        buffer[offset++] = temp & 0xFF;
        buffer[offset++] = (temp >> 8) & 0xFF;
    }

    // ----- 单体电压信息：先写 cell_count（实际电压个数）-----
    buffer[offset++] = cell->cell_count;

    // ----- 再写前 cell->cell_count 个电压（每个 uint16_t 小端序）-----
    for (i = 0; i < cell->cell_count && i < BMS_MAX_CELLS; i++) {
        temp = cell->voltages[i];
        buffer[offset++] = temp & 0xFF;
        buffer[offset++] = (temp >> 8) & 0xFF;
    }

    return offset;
}
