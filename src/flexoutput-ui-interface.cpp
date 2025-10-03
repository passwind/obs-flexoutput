#include "flexoutput-ui-interface.h"
#include "ui/flexoutput-main-window.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include "plugin-support.h"

static FlexOutputMainWindow *g_main_window = nullptr;

// Callback function for menu item
static void menu_callback(void *private_data __attribute__((unused)))
{
    flexoutput_ui_show();
}

void flexoutput_ui_init(void)
{
    obs_log(LOG_INFO, "[FlexOutput UI] Starting UI initialization");
    
    if (!g_main_window) {
        g_main_window = new FlexOutputMainWindow();
        obs_log(LOG_INFO, "[FlexOutput UI] Main window created");
    }
    
    // Add menu item to Tools menu
    obs_frontend_add_tools_menu_item("FlexOutput Settings", menu_callback, nullptr);
    obs_log(LOG_INFO, "[FlexOutput UI] Menu item 'FlexOutput Settings' added to Tools menu");
}

void flexoutput_ui_cleanup(void)
{
    if (g_main_window) {
        delete g_main_window;
        g_main_window = nullptr;
    }
}

void flexoutput_ui_show(void)
{
    if (g_main_window) {
        g_main_window->show();
        g_main_window->raise();
        g_main_window->activateWindow();
    }
}

void flexoutput_ui_hide(void)
{
    if (g_main_window) {
        g_main_window->hide();
    }
}
