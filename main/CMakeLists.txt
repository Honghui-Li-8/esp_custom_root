set(srcs
        "board.c")

idf_component_register(SRCS "main.c" "ble_mesh_config.c" "${srcs}"
                    INCLUDE_DIRS  ".")
