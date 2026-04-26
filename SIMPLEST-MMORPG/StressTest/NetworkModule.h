#pragma once
#include <atomic>

void InitializeNetwork();
void ShutdownNetwork();
void GetPointCloud(int* size, float** points);

extern int global_delay;
extern std::atomic_int active_clients;
