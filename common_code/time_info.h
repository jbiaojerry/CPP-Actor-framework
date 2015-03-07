#ifndef __TIME_INFO_H
#define __TIME_INFO_H

/*!
@brief 启用高精度时钟
*/
void enable_high_resolution();

/*!
@brief 程序优先权提升为“软实时”
*/
void enable_realtime_priority();

/*!
@brief 获取硬件时间戳
*/
long long get_tick_us();
long long get_tick_ms();
int get_tick_s();

#endif