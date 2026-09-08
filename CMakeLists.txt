cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
  set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mod.json" RL_MOD_JSON)
string(JSON RL_MOD_VERSION GET "${RL_MOD_JSON}" "version")
string(REGEX MATCHALL "[0-9]+" RL_MOD_VERSION_LIST ${RL_MOD_VERSION})
list(POP_FRONT RL_MOD_VERSION_LIST
  RL_VERSION_MAJOR
  RL_VERSION_MINOR
  RL_VERSION_PATCH
)
set(RL_VERSION "${RL_VERSION_MAJOR}.${RL_VERSION_MINOR}.${RL_VERSION_PATCH}")

project(RatedLayouts VERSION ${RL_VERSION})

option(RL_DEV "Enable RatedLayouts dev mode" OFF)
set(ARC_FEATURE_TIME ON CACHE BOOL "" FORCE)

if (NOT DEFINED ENV{GEODE_SDK})
  message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
  message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

CPMAddPackage("gh:GlobedGD/argon@1.5.1")
CPMAddPackage("gh:dankmeme01/cue#211f644")
CPMAddPackage("gh:greg7mdp/gtl#f35ce6f")

configure_file(
  "src/utils/Version.hpp.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/Version.hpp"
  @ONLY
)

add_library(RatedLayouts SHARED src/main.cpp)
target_include_directories(RatedLayouts PUBLIC "src" "src/include" "$ENV{GEODE_SDK}/loader/include")
target_link_libraries(RatedLayouts PUBLIC mat-json GeodeResult argon cue gtl)
if(RL_DEV)
  target_compile_definitions(RatedLayouts PUBLIC RL_DEV=1)
endif(RL_DEV)

function(rl_get_sources name)
  set(LIST_SRCS ${ARGN})
  list(TRANSFORM LIST_SRCS PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/src/")
  set(${name} "${LIST_SRCS}" PARENT_SCOPE)
endfunction()

function(add_rl_library name)
  set(library_name "RatedLayouts${name}")
  # Get sources
  string(TOUPPER "${name}_SOURCES" name_sources)
  rl_get_sources(name_SOURCES ${ARGN})
  set(${name_sources} ${name_SOURCES} PARENT_SCOPE)
  # Create the module
  target_sources(RatedLayouts PRIVATE ${name_SOURCES})
endfunction(add_rl_library)

add_rl_library(Utils
  "custom/LazyListButtonPage.cpp"
  "custom/RLAchievementCell.cpp"
  "custom/RLNameplateItem.cpp"
  "custom/RLShopkeeperSprite.cpp"
  "include/RLAchievements.cpp"
  "include/RLConstants.cpp"
  "include/RLDialogIcons.cpp"
  "include/RLLayerBackground.cpp"
  "include/RLLevelInfo.cpp"
  "include/RLNetworkUtils.cpp"
  "include/RLRubyUtils.cpp"
  "utils/CachedSettings.cpp"
  "utils/Cast.cpp"
  "utils/LazyNameplate.cpp"
  "utils/ProxyNode.cpp"
  "utils/RandomGen.cpp"
  "utils/RLArgon.cpp"
  "utils/RLData.cpp"
  "utils/RLSettingsV3.cpp"
  "utils/RLUpdateAccountButtonSettingV3.cpp"
  "utils/TimedCacheEntry.cpp"
)

add_rl_library(Layer
  "layer/RLGauntletLevelsLayer.cpp"
  "layer/RLGauntletSelectLayer.cpp"
  "layer/RLLeaderboardLayer.cpp"
  "layer/RLLevelBrowserLayer.cpp"
  "layer/RLMenuLayer.cpp"
  "layer/RLSearchLayer.cpp"
  "layer/RLSecretLayer1.cpp"
  "layer/RLShopLayer.cpp"
  "layer/RLShopLayer2.cpp"
  "layer/RLSpireLayer.cpp"
  "layer/RLSpireSelectLevelLayer.cpp"
)

add_rl_library(Level
  "level/RLCommunityVotePopup.cpp"
  "level/RLEventLayouts.cpp"
  "level/RLModRatePayloadBuilder.cpp"
  "level/RLModRatePopup.cpp"
  "level/RLModsNotesPopup.cpp"
  "level/RLNotificationAlert.cpp"
  "level/RLNotificationOverlay.cpp"
  "level/RLReportPopup.cpp"
  "level/RLSelectSends.cpp"
  "player/RLDifficultyTotalPopup.cpp"
  "player/RLUserControl.cpp"
  "player/RLUserLevelControl.cpp"
)

add_rl_library(Popup
  "popup/PeekSettingsPopup.cpp"
  "popup/RLAchievementsPopup.cpp"
  "popup/RLAddCodePopup.cpp"
  "popup/RLAddDialogue.cpp"
  "popup/RLAdminNameplatePopup.cpp"
  "popup/RLAnnouncementPopup.cpp"
  "popup/RLBadgeRequestPopup.cpp"
  "popup/RLBuyItemPopup.cpp"
  "popup/RLCreditsPopup.cpp"
  "popup/RLDonationPopup.cpp"
  "popup/RLGuideInfoPopup.cpp"
  "popup/RLNameplateSubmitPopup.cpp"
  "popup/RLNewsAnnouncementPopup.cpp"
  "popup/RLQueueLevelPopup.cpp"
  "popup/RLRubiesCodePopup.cpp"
)

add_rl_library(Hooks
  "hook/Badges.cpp"
  "hook/CommentCell.cpp"
  "hook/CreatorLayer.cpp"
  "hook/DialogLayer.cpp"
  "hook/EndLevelLayer.cpp"
  "hook/GameObjectCoin.cpp"
  "hook/GauntletSelectLayer.cpp"
  "hook/GjGarageLayer.cpp"
  "hook/InfoLayer.cpp"
  "hook/LevelCell.cpp"
  "hook/LevelInfoLayer.cpp"
  "hook/LevelSearchLayer.cpp"
  "hook/MenuLayer.cpp"
  "hook/PlayLayer.cpp"
  "hook/ProfilePage.cpp"
)

setup_geode_mod(RatedLayouts LINK_TYPE PRIVATE)
