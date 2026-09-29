#pragma once
struct SpiMock { void beginTransaction(int) {} void endTransaction() {} };
inline SpiMock SPI;
#define SPI_ETHERNET_SETTINGS 0
