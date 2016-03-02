#include "windows.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef SUCCEEDED
#undef FAILED

#include "asset_manager.hpp"
#include "platform/os.hpp"

// Stub implementation.

namespace MaliSDK
{
AssetManager &OS::getAssetManager()
{
	return *static_cast<AssetManager *>(nullptr);
}

unsigned OS::getNumberOfCpuThreads()
{
	return 1;
}

double OS::getCurrentTime()
{
	return 0.0;
}
}

int main()
{
}