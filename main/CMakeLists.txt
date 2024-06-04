set(srcs
        "board.c")

idf_component_register(SRCS "ble_mesh_config_root.c" "main.c" "${srcs}"
                    INCLUDE_DIRS  ".")