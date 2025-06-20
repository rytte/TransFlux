#include "Tool.h"

// 处理 Annex-B 格式流，判断帧类型
void check_frame_type(uint8_t *data, int size)
{
    int offset = 0;
    while (offset < size)
    {
        // 查找每个 NALU，Nalu 以 0x00000001 或 0x000001 开始
        if (offset + 4 <= size && (data[offset] == 0x00 && data[offset + 1] == 0x00 && data[offset + 2] == 0x00 && data[offset + 3] == 0x01))
        {
            offset += 4;  // 跳过起始码（0x00000001）

            // 读取 NALU 数据
            uint8_t *nalu_data = data + offset;
            int nalu_length = 0;

            // 查找下一个起始码（NALU 结束位置）
            while (offset + 4 <= size && !(data[offset] == 0x00 && data[offset + 1] == 0x00 && data[offset + 2] == 0x00 && data[offset + 3] == 0x01))
            {
                offset++;
                nalu_length++;
            }

            // 解析 NALU 类型
            int slice_type = nalu_data[0] & 0x1F;

            switch (slice_type)
            {
            case 1: logger.warning("Slice, contain P/B"); break;
            case 5: logger.warning("I-frame"); break;
            case 7: logger.warning("SPS"); break;
            case 8: logger.warning("PPS"); break;
            default: logger.warning("unknow");
            }

            // 继续处理下一个 NALU
            offset += nalu_length;
        }
        else
            offset++;
    }
}

void printH264Stream(const char* inputFile, int printSize)
{
    FILE* fd = fopen(inputFile, "rb");
    if (fd == nullptr)
    {
        logger.error("Failed to open file: %s", inputFile);
        return;
    }

    int64_t readSize = 0;
    uint8_t* buf = (uint8_t*)malloc(512);
    int index = 0;

    while (printSize > 0)
    {
        readSize = fread(buf, 1, 512, fd);
        if (readSize <= 0)
            break;

        if (readSize > printSize)
            readSize = printSize;

        for (int i = 0; i < readSize; i+=4)
        {
            if (i + 4 > readSize)   // 不满四个直接舍弃
                break;

            logger.warning("data[%d]: 0x%02X, 0x%02X, 0x%02X, 0x%02X", index, buf[i], buf[i+1], buf[i+2], buf[i+3]);

            index += 4;
        }

        printSize -= readSize;
    }

    free(buf);
    fclose(fd);
}
