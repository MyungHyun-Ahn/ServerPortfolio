#pragma once

#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "MHLib/utils/CFileLoader.h"
#include "MHLib/security/CEncryption.h"
#include "NetworkLib/CoreUtils.h"
#include "EchoProtocol.h"
