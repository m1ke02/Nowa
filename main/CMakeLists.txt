idf_component_register(
SRCS
    "main.cpp"
    "tcp_server.c"
    "connect.c"
    "protocol_examples_utils.c"
    "wifi_connect.c"
    "emci_profile.cpp"
    "uart_console.c"
    "web_server.c"
    "web_server_profile.cpp"
    "netlog.c"
    "spiffs.c"

    "../Emci/src/emci_arg.c"
    "../Emci/src/emci_parser.c"
    "../Emci/src/emci_std_handlers.c"

    "ble_ancs/ble_ancs_utils.c"
    "ble_ancs/ble_ancs.c"
    "ble_ancs/ble_utils.c"

    "dispatcher/Dispatcher.cpp"
    "dispatcher/DispatcherDriverInterface.cpp"
    "dispatcher/DispatcherUtils.cpp"
    "dispatcher/NotificationProvider.cpp"

INCLUDE_DIRS
    "include"
    "../Emci/include"

    "ble_ancs/include"

    "dispatcher/include"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++20)
# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
