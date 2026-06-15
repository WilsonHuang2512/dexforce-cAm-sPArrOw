#pragma once
#include <windows.h>
#include <iostream>


// 初始化临界区
void InitializeCriticalSection();

// 清理临界区资源
void DeleteCriticalSection();

// 进入临界区
void EnterCriticalSection();

//离开临界区
void LeaveCriticalSection();

