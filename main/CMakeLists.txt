idf_component_register(SRC_DIRS
                    "."
                    "./servo"
                    "ui"
                    "ui/layer_manage"
                    "ui/fonts"
                    INCLUDE_DIRS
                    "."
                    "./servo"
                    "ui"
                    "ui/layer_manage")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-cast-function-type)
