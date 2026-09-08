#pragma once

namespace rl {

/// In `utils/CachedSettings.cpp`
void CachedSettings_init();
/// In `utils/RLData.cpp`
void RLData_init();
/// In `layer/RLShopLayer2.cpp`
void ShopLayer_init(); // Get the sizes at startup
void ShopLayer_prefetch(); // Start loading the pages when checking the main menu
/// In `hooks/Badges.cpp`
bool Badges_init();

} // namespace rl
