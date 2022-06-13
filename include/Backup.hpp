#pragma once

#include <vector>
#include <functional>

std::function<void()> GetBackupFunction();

void ShowBackupDialog(std::function<void()> backupFunction);
