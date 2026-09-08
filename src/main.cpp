#include "RLConstants.hpp"
#include "RLNetworkUtils.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/RandomGen.hpp"
#include "utils/RLArgon.hpp"
#include "utils/RLData.hpp"
#include "utils/StartupFunctions.hpp"
#include <atomic>
#include <fmt/ranges.h>
#include <Geode/DefaultInclude.hpp>
#include <Geode/Geode.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/modify/AccountHelpLayer.hpp>
#include <Geode/modify/AccountLoginLayer.hpp>
#include <Geode/modify/SupportLayer.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/terminate.hpp>
#include <arc/future/Future.hpp>

using namespace geode::prelude;
using namespace rl;
using namespace std::literals::string_view_literals;

// TODO: Check on random performance issues
// TODO: Check on hangs when closing game

static constexpr std::string_view OldSavedValuesToRemove[]{"isClassicMod",
                                                           "isClassicAdmin",
                                                           "isLeaderboardMod",
                                                           "isLeaderboardAdmin",
                                                           "isPlatMod",
                                                           "isPlatAdmin",
                                                           "isOwner",
                                                           "isDeveloper",
                                                           //"masterKey",
                                                           "argon_token"};

// Clear settings that are now unused.
static void removeOldSavedValues() {
    matjson::Value& values = Mod::get()->getSaveContainer();
    for (std::string_view saved : OldSavedValuesToRemove) {
        if (values.erase(saved)) log::debug("cleared saved value \"{}\"", saved);
    }
}

$on_mod(Loaded) {
    RLArgon::authorize(true);
    // Other initialization
    rl::CachedSettings_init();
    rl::RLData_init();

    GJUserScore* userScore = GJUserScore::create();
    userScore->m_accountID = GJAccountManager::get()->m_accountID;
    if (userScore->m_modBadge != 0) {
        log::debug("User has mod badge: {}", userScore->m_modBadge);
        // TODO: Change this?
        Mod::get()->setSettingValue<bool>("disableRLReq", true);
        CachedSettings::get()->disableRLReq = true;
        CachedSettings::get()->isUserGDMod = true;
    } else {
        log::debug("User does not have a mod badge");
    }

    // disable rl req if is a gdps
    if (rl::isGDPS()) {
        log::debug("Running on GDPS, disabling Rated Layouts requests");
        Mod::get()->setSettingValue<bool>("disableRLReq", true);
        CachedSettings::get()->disableRLReq = true;
    }

    // Set up badgify/badgified if the mod and repo are available.
    rl::Badges_init();
    // Prefetch shop items.
    rl::ShopLayer_init();
    // Clear old/unused settings from the config.
    // TODO: Remove in update v1.0.17?
    removeOldSavedValues();
}

class $modify(RLSupportLayer, SupportLayer) {
    struct Fields {
        async::TaskHolder<web::WebResponse> m_getAccessTask;
        async::TaskHolder<Result<std::string>> m_authTask;
        CCMenu* m_argonMenu = nullptr;
        CCMenuItemSpriteExtra* m_argonButton = nullptr;
        ~Fields() {
            m_getAccessTask.cancel();
            m_authTask.cancel();
        }
    };

    void setupArgonButton() {
        if (CachedSettings::get()->disableRLReq) return;
        if (m_fields->m_argonButton || !m_fields->m_argonMenu) return;
        if (rl::isUserHasPerms() || rl::isUserOwner() || rl::isUserDeveloper()) {
            auto* argonBtnSpr = ButtonSprite::create(
                "Argon", 25, true, "bigFont.fnt", "GJ_button_04.png", 25.f, 1.f);
            auto* argonBtn = CCMenuItemSpriteExtra::create(
                argonBtnSpr, this, menu_selector(RLSupportLayer::onGetArgon));
            m_fields->m_argonButton = argonBtn;
            argonBtn->setPosition({328, 85});
            m_fields->m_argonMenu->addChild(argonBtn);
        }
    }

    void customSetup() {
        if (!CachedSettings::get()->disableRLReq) {
            CCMenu* argonMenu = CCMenu::create();
            m_fields->m_argonMenu = argonMenu;
            argonMenu->setPosition({0, 0});
            m_listLayer->addChild(argonMenu, 10);
            // show the argon button for user with a role
            auto keyBtnSpr =
                ButtonSprite::create("Key", 25, true, "bigFont.fnt", "GJ_button_04.png", 25.f, 1.f);
            auto keyBtn = CCMenuItemSpriteExtra::create(
                keyBtnSpr, this, menu_selector(RLSupportLayer::onImportKey));
            keyBtn->setPosition({328, 55});
            argonMenu->addChild(keyBtn);
            this->setupArgonButton();
        }

        SupportLayer::customSetup();
    }

    void onGetArgon(CCObject* sender) {
        m_uploadPopup = UploadActionPopup::create(nullptr, "Obtaining Token...");
        m_uploadPopup->show();
        if (!RLArgon::hasToken()) {
            RLArgon::wait();
        }
        Ref<UploadActionPopup> popupRef = m_uploadPopup;
        m_fields->m_authTask.spawn(argon::startAuth(), [this, popupRef](Result<std::string> res) {
            if (!popupRef) return;
            if (res.isOk()) {
                std::string token = std::move(res).unwrap();
                utils::clipboard::write(token);
                popupRef->showSuccessMessage("Token copied to clipboard.");
            } else {
                std::string err = std::move(res).unwrapErr();
                if (!err.empty()) {
                    log::warn("Auth failed: {}", err);
                    popupRef->showFailMessage(std::move(err));
                } else {
                    log::warn("Auth failed: Reason unknown");
                    popupRef->showFailMessage("Argon auth failed! Try again later.");
                }
                RLArgon::clear();
            }
        });
    }

    void onImportKey(CCObject* sender) {
        m_uploadPopup = UploadActionPopup::create(nullptr, "Importing key...");
        m_uploadPopup->show();

        WeakRef<RLSupportLayer> weak = this;
        Ref<UploadActionPopup> popupRef = m_uploadPopup;
        async::spawn([weak, popupRef]() -> arc::Future<> {
            utils::file::FilePickOptions options;
            options.filters.push_back({"Private Key", {"*.ppk", "*.pem"}});
            auto result = co_await utils::file::pick(utils::file::PickMode::OpenFile, options);

            auto failMessage = [&](gd::string message) {
                Loader::get()->queueInMainThread([weak, popupRef, msg = std::move(message)]() {
                    if (weak.lock()) popupRef->showFailMessage(std::move(msg));
                });
            };

            if (!result) {
                failMessage("Failed to open file picker");
                co_return;
            }

            auto maybePath = result.unwrap();
            if (!maybePath) {
                failMessage("No file selected");
                co_return;
            }

            auto path = *maybePath;
            auto textRes = utils::file::readString(utils::string::pathToString(path));
            if (!textRes) {
                failMessage("Failed to read key file");
                co_return;
            }

            auto key = textRes.unwrap();
            if (key.empty()) {
                failMessage("Selected key file is empty");
                co_return;
            }

            geode::ByteVector keyBytes(key.begin(), key.end());
            web::MultipartForm form;
            form.file("privateKey", keyBytes, path.filename().string(), "application/octet-stream");

            auto postReq = web::WebRequest();
            postReq.bodyMultipart(form);

            auto response = co_await postReq.post(std::string(rl::BASE_API_URL) + "/masterKey");
            if (!weak.lock()) co_return;

            if (!response.ok()) {
                std::string errorMsg =
                    rl::getResponseFailMessage(response, "Failed to upload private key");
                failMessage(std::move(errorMsg));
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                if (auto optExtraCtx = jsonRes.err())
                    failMessage("Invalid server response: " + *optExtraCtx);
                else
                    failMessage("Invalid server response");
                co_return;
            }

            auto json = std::move(jsonRes).unwrap();
            std::string returnedKey = json["masterKey"].asString().unwrapOr("");
            if (returnedKey.empty()) {
                std::string errorMsg = json["message"].asString().unwrapOr(
                    "Private key rejected or missing master key");
                failMessage(std::move(errorMsg));
                co_return;
            }

            Loader::get()->queueInMainThread([popupRef, returnedKey = std::move(returnedKey)]() {
                Mod::get()->setSavedValue("masterKey", returnedKey);
                if (popupRef) {
                    popupRef->showSuccessMessage("Private key accepted.");
                    clipboard::write(returnedKey);
                }
            });
            co_return;
        });
    }

    // TODO: Handle cases if someone DOES get mod fsr?
    void onRequestAccess(CCObject* sender) {  // i assume that no one will ever get gd mod xddd
        if (CachedSettings::get()->disableRLReq) {
            SupportLayer::onRequestAccess(sender);
            return;
        }

        m_uploadPopup = UploadActionPopup::create(nullptr, "Loading...");
        m_uploadPopup->show();

        if (!RLArgon::hasToken()) {
            RLArgon::wait();
            if (RLArgon::failed()) {
                m_uploadPopup->showFailMessage(RLArgon::failureMessage());
                return;
            }
        }

        Ref<RLSupportLayer> self = this;
        Ref<UploadActionPopup> popupRef = m_uploadPopup;

        // json body
        const int accountId = GJAccountManager::get()->m_accountID;
        matjson::Value jsonBody = matjson::Value::object();
        jsonBody["argonToken"] = RLArgon::token();
        jsonBody["accountId"] = accountId;
        auto masterKey = Mod::get()->getSavedValue<std::string>("masterKey");
        if (!masterKey.empty()) {
            jsonBody["masterKey"] = masterKey;
        }

        // verify the user's role
        auto postReq = web::WebRequest();
        postReq.bodyJSON(jsonBody);

        m_fields->m_getAccessTask.spawn(
            postReq.post(std::string(rl::BASE_API_URL) + "/getAccess"),
            [this, self, popupRef, accountId](web::WebResponse response) {
            log::trace("onRequestAccess: Received response from server");
            if (!response.ok()) {
                log::warn("onRequestAccess: Server returned non-ok status: {}", response.code());
                popupRef->showFailMessage(
                    rl::getResponseFailMessage(response, "Failed! Try again later."));
                return;
            }

            auto jsonRes = response.json();
            if (!jsonRes) {
                log::warn("onRequestAccess: Failed to parse JSON response");
                popupRef->showFailMessage("Failed to parse JSON response");
                return;
            }

            auto json = std::move(jsonRes).unwrap();
            auto info = json.as<RLUserRoles>().unwrapOrDefault();

            auto CS = CachedSettings::update();
            auto& userData = CS->userData;
            if (userData.accountId != accountId) {
                // TODO: Handle this case better...
                log::error("Incorrect account id, clearing cache");
                userData.clear();
                userData.accountId = accountId;
            }
            userData.init(info);
            self->setupArgonButton();

            int roleCount = 0;
            roleCount += info.isClassicMod ? 1 : 0;
            roleCount += info.isClassicAdmin ? 1 : 0;
            roleCount += info.isLeaderboardMod ? 1 : 0;
            roleCount += info.isLeaderboardAdmin ? 1 : 0;
            roleCount += info.isPlatMod ? 1 : 0;
            roleCount += info.isPlatAdmin ? 1 : 0;
            roleCount += info.isDeveloper ? 1 : 0;
            roleCount += info.isOwner ? 1 : 0;

            if (roleCount > 1) {
                log::info("Granted Multiple roles");
                popupRef->showSuccessMessage("Granted Layout roles.");
            } else if (info.isClassicMod) {
                log::info("Granted Layout Mod role");
                popupRef->showSuccessMessage("Granted Classic Layout Mod.");
            } else if (info.isClassicAdmin) {
                log::info("Granted Layout Admin role");
                popupRef->showSuccessMessage("Granted Classic Layout Admin.");
            } else if (info.isLeaderboardMod) {
                log::info("Granted Leaderboard Layout Mod role");
                popupRef->showSuccessMessage("Granted Leaderboard Layout Mod.");
            } else if (info.isLeaderboardAdmin) {
                log::info("Granted Leaderboard Layout Admin role");
                popupRef->showSuccessMessage("Granted Leaderboard Layout Admin.");
            } else if (info.isPlatMod) {
                log::info("Granted Platformer Layout Mod role");
                popupRef->showSuccessMessage("Granted Platformer Layout Mod.");
            } else if (info.isPlatAdmin) {
                log::info("Granted Platformer Admin role");
                popupRef->showSuccessMessage("Granted Platformer Layout Admin.");
            } else if (info.isOwner) {
                log::info("Granted Owner role");
                popupRef->showSuccessMessage("Granted Owner role.");
            } else if (info.isDeveloper) {
                log::info("Granted Developer role");
                popupRef->showSuccessMessage("Granted Developer role.");
            } else {
                popupRef->showFailMessage("Failed! Nothing found.");
            }
        });
    }
};

class $modify(AccountHelpLayer) {
    void accountStatusChanged() override {
        AccountHelpLayer::accountStatusChanged();
        rl::updateCachedUserData();
        RLArgon::clear();
    }
};

class $modify(AccountLoginLayer) {
    void loginAccountFinished(int accountID, int userID) override {
        AccountLoginLayer::loginAccountFinished(accountID, userID);
        rl::updateCachedUserData();
        RLArgon::authorize();
    }
};
