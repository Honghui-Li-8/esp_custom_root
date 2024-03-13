set(srcs
        "board.c")

idf_component_register(SRCS "old_ble_mesh_config_root.c" "ble_mesh_config_root.c" "main.c" "${srcs}"
                    INCLUDE_DIRS  ".")
