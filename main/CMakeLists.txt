file(GLOB IMG_SRCS "img/*.c")
file(GLOB_RECURSE MASCOT_SRCS "img/time_page_mascot/*.c")

idf_component_register(
    SRCS "bedstead.cpp" "main.cpp" "page.cpp" "pillow.cpp" ${IMG_SRCS} ${MASCOT_SRCS}
    INCLUDE_DIRS "." "img" "img/time_page_mascot")

set_source_files_properties(
    ${LV_DEMOS_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE)
