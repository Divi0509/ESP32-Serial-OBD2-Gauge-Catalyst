SET(SOURCES ui_Screen_ScreenTitle.c
    ui_Screen_ScreenGauge.c
    ui_Screen_ScreenMonitor.c
    ui_Screen_ScreenConfig.c
    ui_Screen_ScreenService.c
    ui_Screen_ScreenPerformance.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_vaandcob_png.c
    ui_img_gauge3_png.c
    ui_img_ledring_png.c
    ui_img_moon_png.c
    ui_img_sun_png.c
    ui_img_thailand_png.c
    ui_img_qrcode_png.c
    ui_img_mil_png.c
    ui_img_alert_png.c
    fonts/ui_font_oswald_36.c
    fonts/ui_font_segment_40.c)

add_library(ui ${SOURCES})
