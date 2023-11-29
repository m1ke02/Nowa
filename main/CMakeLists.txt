idf_component_register(
SRCS
    "main.cpp"
    "tcp_server.c"
    "connect.c"
    "protocol_examples_utils.c"
    "wifi_connect.c"
    "emci_profile.c"
    "dispatcher.cpp"

    "../Emci/src/emci_arg.c"
    "../Emci/src/emci_parser.c"
    "../Emci/src/emci_std_handlers.c"

    "ble_ancs/ble_ancs_utils.c"
    "ble_ancs/ble_ancs.c"
    "ble_ancs/ble_utils.c"

INCLUDE_DIRS
    "include"
    "../Emci/include"

    "ble_ancs/include"
)
