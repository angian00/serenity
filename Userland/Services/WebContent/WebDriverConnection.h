/*
 * Copyright (c) 2022, Florent Castelli <florent.castelli@gmail.com>
 * Copyright (c) 2022, Linus Groh <linusg@serenityos.org>
 * Copyright (c) 2022, Tim Flynn <trflynn89@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Function.h>
#include <AK/HashMap.h>
#include <AK/String.h>
#include <LibIPC/ConnectionToServer.h>
#include <LibJS/Forward.h>
#include <LibJS/Heap/MarkedVector.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebDriver/ElementLocationStrategies.h>
#include <LibWeb/WebDriver/Response.h>
#include <LibWeb/WebDriver/TimeoutsConfiguration.h>
#include <WebContent/Forward.h>
#include <WebContent/WebDriverClientEndpoint.h>
#include <WebContent/WebDriverServerEndpoint.h>

namespace WebContent {

class WebDriverConnection final
    : public IPC::ConnectionToServer<WebDriverClientEndpoint, WebDriverServerEndpoint> {
    C_OBJECT(WebDriverConnection)

public:
    static ErrorOr<NonnullRefPtr<WebDriverConnection>> connect(Web::PageClient& page_client, String const& webdriver_ipc_path);
    virtual ~WebDriverConnection() = default;

private:
    WebDriverConnection(NonnullOwnPtr<Core::Stream::LocalSocket> socket, Web::PageClient& page_client);

    virtual void die() override { }

    virtual void close_session() override;
    virtual void set_page_load_strategy(Web::WebDriver::PageLoadStrategy const& page_load_strategy) override;
    virtual void set_unhandled_prompt_behavior(Web::WebDriver::UnhandledPromptBehavior const& unhandled_prompt_behavior) override;
    virtual void set_strict_file_interactability(bool strict_file_interactability) override;
    virtual void set_is_webdriver_active(bool) override;
    virtual Messages::WebDriverClient::GetTimeoutsResponse get_timeouts() override;
    virtual Messages::WebDriverClient::SetTimeoutsResponse set_timeouts(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::NavigateToResponse navigate_to(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::GetCurrentUrlResponse get_current_url() override;
    virtual Messages::WebDriverClient::BackResponse back() override;
    virtual Messages::WebDriverClient::ForwardResponse forward() override;
    virtual Messages::WebDriverClient::RefreshResponse refresh() override;
    virtual Messages::WebDriverClient::GetTitleResponse get_title() override;
    virtual Messages::WebDriverClient::GetWindowHandleResponse get_window_handle() override;
    virtual Messages::WebDriverClient::CloseWindowResponse close_window() override;
    virtual Messages::WebDriverClient::GetWindowHandlesResponse get_window_handles() override;
    virtual Messages::WebDriverClient::GetWindowRectResponse get_window_rect() override;
    virtual Messages::WebDriverClient::SetWindowRectResponse set_window_rect(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::MaximizeWindowResponse maximize_window() override;
    virtual Messages::WebDriverClient::MinimizeWindowResponse minimize_window() override;
    virtual Messages::WebDriverClient::FullscreenWindowResponse fullscreen_window() override;
    virtual Messages::WebDriverClient::FindElementResponse find_element(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::FindElementsResponse find_elements(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::FindElementFromElementResponse find_element_from_element(JsonValue const& payload, String const& element_id) override;
    virtual Messages::WebDriverClient::FindElementsFromElementResponse find_elements_from_element(JsonValue const& payload, String const& element_id) override;
    virtual Messages::WebDriverClient::FindElementFromShadowRootResponse find_element_from_shadow_root(JsonValue const& payload, String const& shadow_id) override;
    virtual Messages::WebDriverClient::FindElementsFromShadowRootResponse find_elements_from_shadow_root(JsonValue const& payload, String const& shadow_id) override;
    virtual Messages::WebDriverClient::GetActiveElementResponse get_active_element() override;
    virtual Messages::WebDriverClient::GetElementShadowRootResponse get_element_shadow_root(String const& element_id) override;
    virtual Messages::WebDriverClient::IsElementSelectedResponse is_element_selected(String const& element_id) override;
    virtual Messages::WebDriverClient::GetElementAttributeResponse get_element_attribute(String const& element_id, String const& name) override;
    virtual Messages::WebDriverClient::GetElementPropertyResponse get_element_property(String const& element_id, String const& name) override;
    virtual Messages::WebDriverClient::GetElementCssValueResponse get_element_css_value(String const& element_id, String const& name) override;
    virtual Messages::WebDriverClient::GetElementTextResponse get_element_text(String const& element_id) override;
    virtual Messages::WebDriverClient::GetElementTagNameResponse get_element_tag_name(String const& element_id) override;
    virtual Messages::WebDriverClient::GetElementRectResponse get_element_rect(String const& element_id) override;
    virtual Messages::WebDriverClient::IsElementEnabledResponse is_element_enabled(String const& element_id) override;
    virtual Messages::WebDriverClient::GetSourceResponse get_source() override;
    virtual Messages::WebDriverClient::ExecuteScriptResponse execute_script(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::ExecuteAsyncScriptResponse execute_async_script(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::GetAllCookiesResponse get_all_cookies() override;
    virtual Messages::WebDriverClient::GetNamedCookieResponse get_named_cookie(String const& name) override;
    virtual Messages::WebDriverClient::AddCookieResponse add_cookie(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::DeleteCookieResponse delete_cookie(String const& name) override;
    virtual Messages::WebDriverClient::DeleteAllCookiesResponse delete_all_cookies() override;
    virtual Messages::WebDriverClient::DismissAlertResponse dismiss_alert() override;
    virtual Messages::WebDriverClient::AcceptAlertResponse accept_alert() override;
    virtual Messages::WebDriverClient::GetAlertTextResponse get_alert_text() override;
    virtual Messages::WebDriverClient::SendAlertTextResponse send_alert_text(JsonValue const& payload) override;
    virtual Messages::WebDriverClient::TakeScreenshotResponse take_screenshot() override;
    virtual Messages::WebDriverClient::TakeElementScreenshotResponse take_element_screenshot(String const& element_id) override;
    virtual Messages::WebDriverClient::PrintPageResponse print_page() override;

    ErrorOr<void, Web::WebDriver::Error> ensure_open_top_level_browsing_context();
    ErrorOr<void, Web::WebDriver::Error> handle_any_user_prompts();
    void restore_the_window();
    Gfx::IntRect maximize_the_window();
    Gfx::IntRect iconify_the_window();

    using StartNodeGetter = Function<ErrorOr<Web::DOM::ParentNode*, Web::WebDriver::Error>()>;
    ErrorOr<JsonArray, Web::WebDriver::Error> find(StartNodeGetter&& start_node_getter, Web::WebDriver::LocationStrategy using_, StringView value);

    struct ScriptArguments {
        String script;
        JS::MarkedVector<JS::Value> arguments;
    };
    ErrorOr<ScriptArguments, Web::WebDriver::Error> extract_the_script_arguments_from_a_request(JsonValue const& payload);
    void delete_cookies(Optional<StringView> const& name = {});

    Web::PageClient& m_page_client;

    // https://w3c.github.io/webdriver/#dfn-page-load-strategy
    Web::WebDriver::PageLoadStrategy m_page_load_strategy { Web::WebDriver::PageLoadStrategy::Normal };

    // https://w3c.github.io/webdriver/#dfn-unhandled-prompt-behavior
    Web::WebDriver::UnhandledPromptBehavior m_unhandled_prompt_behavior { Web::WebDriver::UnhandledPromptBehavior::DismissAndNotify };

    // https://w3c.github.io/webdriver/#dfn-strict-file-interactability
    bool m_strict_file_interactability { false };

    // https://w3c.github.io/webdriver/#dfn-session-script-timeout
    Web::WebDriver::TimeoutsConfiguration m_timeouts_configuration;

    struct Window {
        String handle;
        bool is_open { false };
    };
    HashMap<String, Window> m_windows;
    String m_current_window_handle;
};

}
