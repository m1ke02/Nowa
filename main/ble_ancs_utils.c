#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "ble_ancs_utils.h"
#include "ancs.h"

#define TAG "ANCS"

/*
| EventID(1 Byte) | EventFlags(1 Byte) | CategoryID(1 Byte) | CategoryCount(1 Byte) | NotificationUID(4 Bytes) |

A GATT notification delivered through the Notification Source characteristic contains the following information:
* EventID: This field informs the accessory whether the given iOS notification was added, modified, or removed. The enumerated values for this field are defined
            in EventID Values.
* EventFlags: A bitmask whose set bits inform an NC of specificities with the iOS notification. For example, if an iOS notification is considered “important”,
              the NC may want to display a more aggressive user interface (UI) to make sure the user is properly alerted. The enumerated bits for this field
              are defined in EventFlags.
* CategoryID: A numerical value providing a category in which the iOS notification can be classified. The NP will make a best effort to provide an accurate category
              for each iOS notification. The enumerated values for this field are defined in CategoryID Values.
* CategoryCount: The current number of active iOS notifications in the given category. For example, if two unread emails are sitting in a user’s email inbox, and a new
                 email is pushed to the user’s iOS device, the value of CategoryCount is 3.
* NotificationUID: A 32-bit numerical value that is the unique identifier (UID) for the iOS notification. This value can be used as a handle in commands sent to the
                   Control Point characteristic to interact with the iOS notification.
*/

char *EventID_to_String(uint8_t EventID)
{
    char *str = NULL;
    switch (EventID)
    {
        case EventIDNotificationAdded:
            str = "New message";
            break;
        case EventIDNotificationModified:
            str = "Modified message";
            break;
        case EventIDNotificationRemoved:
            str = "Removed message";
            break;
        default:
            str = "unknown EventID";
            break;
    }
    return str;
}

char *CategoryID_to_String(uint8_t CategoryID)
{
    char *Cidstr = NULL;
    switch(CategoryID) {
        case CategoryIDOther:
            Cidstr = "Other";
            break;
        case CategoryIDIncomingCall:
            Cidstr = "IncomingCall";
            break;
        case CategoryIDMissedCall:
            Cidstr = "MissedCall";
            break;
        case CategoryIDVoicemail:
            Cidstr = "Voicemail";
            break;
        case CategoryIDSocial:
            Cidstr = "Social";
            break;
        case CategoryIDSchedule:
            Cidstr = "Schedule";
            break;
        case CategoryIDEmail:
            Cidstr = "Email";
            break;
        case CategoryIDNews:
            Cidstr = "News";
            break;
        case CategoryIDHealthAndFitness:
            Cidstr = "HealthAndFitness";
            break;
        case CategoryIDBusinessAndFinance:
            Cidstr = "BusinessAndFinance";
            break;
        case CategoryIDLocation:
            Cidstr = "Location";
            break;
        case CategoryIDEntertainment:
            Cidstr = "Entertainment";
            break;
        default:
            Cidstr = "Unknown CategoryID";
            break;
    }
    return Cidstr;
}

/*
| EventID(1 Byte) | EventFlags(1 Byte) | CategoryID(1 Byte) | CategoryCount(1 Byte) | NotificationUID(4 Bytes) |
*/

void esp_receive_apple_notification_source(uint8_t *message, uint16_t message_len)
{
    if (!message || message_len < 5) {
        return;
    }

    uint8_t EventID    = message[0];
    char    *EventIDS  = EventID_to_String(EventID);
    uint8_t EventFlags = message[1];
    uint8_t CategoryID = message[2];
    char    *Cidstr    = CategoryID_to_String(CategoryID);
    uint8_t CategoryCount = message[3];
    uint32_t NotificationUID = (message[4]) | (message[5]<< 8) | (message[6]<< 16) | (message[7] << 24);
    ESP_LOGI(TAG, "EventID:%s EventFlags:0x%x CategoryID:%s CategoryCount:%d NotificationUID:%" PRIu32, EventIDS, EventFlags, Cidstr, CategoryCount, NotificationUID);
}

void esp_receive_apple_data_source(uint8_t *message, uint16_t message_len)
{
    ESP_LOGV(TAG, "esp_receive_apple_data_source");

    size_t sent = xMessageBufferSend(ancs_message_buffer, "TestAppleData", 13, pdMS_TO_TICKS(10));
    if (sent != 13) {
        ESP_LOGE(TAG, "xMessageBufferSend failed: sent %u instead of %u", sent, 13);
    }

    //esp_log_buffer_hex("data source", message, message_len);
    if (!message || message_len == 0) {
        return;
    }
    uint8_t Command_id = message[0];
    switch (Command_id)
    {
        case CommandIDGetNotificationAttributes: {
            uint32_t NotificationUID = (message[1]) | (message[2]<< 8) | (message[3]<< 16) | (message[4] << 24);
            uint32_t remian_attr_len = message_len - 5;
            uint8_t *attrs = &message[5];
            ESP_LOGI(TAG, "Notification Attributes response Command_id %d NotificationUID %" PRIu32, Command_id, NotificationUID);
            while(remian_attr_len > 0) {
                uint8_t AttributeID = attrs[0];
                uint16_t len = attrs[1] | (attrs[2] << 8);
                if(len > (remian_attr_len -3)) {
                    ESP_LOGE(TAG, "data error");
                    break;
                }
                switch (AttributeID)
                {
                    case NotificationAttributeIDAppIdentifier:
                        esp_log_buffer_char("Identifier", &attrs[3], len);
                        break;
                    case NotificationAttributeIDTitle:
                        esp_log_buffer_char("Title", &attrs[3], len);
                        break;
                    case NotificationAttributeIDSubtitle:
                        esp_log_buffer_char("Subtitle", &attrs[3], len);
                        break;
                    case NotificationAttributeIDMessage:
                        esp_log_buffer_char("Message", &attrs[3], len);
                        break;
                    case NotificationAttributeIDMessageSize:
                        esp_log_buffer_char("MessageSize", &attrs[3], len);
                        break;
                    case NotificationAttributeIDDate:
                        //yyyyMMdd'T'HHmmSS
                        esp_log_buffer_char("Date", &attrs[3], len);
                        break;
                    case NotificationAttributeIDPositiveActionLabel:
                        esp_log_buffer_hex("PActionLabel", &attrs[3], len);
                        break;
                    case NotificationAttributeIDNegativeActionLabel:
                        esp_log_buffer_hex("NActionLabel", &attrs[3], len);
                        break;
                    default:
                        esp_log_buffer_hex("unknownAttributeID", &attrs[3], len);
                        break;
                }

                attrs += (1 + 2 + len);
                remian_attr_len -= (1 + 2 + len);
            }

            break;
        }
        case CommandIDGetAppAttributes:
            ESP_LOGI(TAG, "APP Attributes response");
            break;
        case CommandIDPerformNotificationAction:
            ESP_LOGI(TAG, "Perform Notification Action");
            break;
        default:
            ESP_LOGI(TAG, "unknown Command ID");
            break;
    }
}

char *Errcode_to_String(uint16_t status)
{
    char *Errstr = NULL;
    switch (status) {
        case Unknown_command:
            Errstr = "Unknown_command";
            break;
        case Invalid_command:
            Errstr = "Invalid_command";
            break;
        case Invalid_parameter:
            Errstr = "Invalid_parameter";
            break;
        case Action_failed:
            Errstr = "Action_failed";
            break;
        default:
            Errstr = "unknown_failed";
            break;
    }
    return Errstr;

}

//!!!

/**@brief Function for encoding a uint16 value.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data is to be written.
 *
 * @return      Number of bytes written.
 */
static uint8_t uint16_encode(uint16_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) ((value & 0x00FF) >> 0);
    p_encoded_data[1] = (uint8_t) ((value & 0xFF00) >> 8);
    return sizeof(uint16_t);
}


/**@brief Function for decoding a uint16 value.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value.
 */
static uint16_t uint16_decode(const uint8_t * p_encoded_data)
{
        return ( (((uint16_t)((uint8_t *)p_encoded_data)[0])) |
                 (((uint16_t)((uint8_t *)p_encoded_data)[1]) << 8 ));
}


/**@brief Function for encoding a uint32 value.
 *
 * @param[in]   value            Value to be encoded.
 * @param[out]  p_encoded_data   Buffer where the encoded data is to be written.
 *
 * @return      Number of bytes written.
 */
static uint8_t uint32_encode(uint32_t value, uint8_t * p_encoded_data)
{
    p_encoded_data[0] = (uint8_t) ((value & 0x000000FF) >> 0);
    p_encoded_data[1] = (uint8_t) ((value & 0x0000FF00) >> 8);
    p_encoded_data[2] = (uint8_t) ((value & 0x00FF0000) >> 16);
    p_encoded_data[3] = (uint8_t) ((value & 0xFF000000) >> 24);
    return sizeof(uint32_t);
}


/**@brief Function for decoding a uint32 value.
 *
 * @param[in]   p_encoded_data   Buffer where the encoded data is stored.
 *
 * @return      Decoded value.
 */
static uint32_t uint32_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint32_t)((uint8_t *)p_encoded_data)[0]) << 0)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[1]) << 8)  |
             (((uint32_t)((uint8_t *)p_encoded_data)[2]) << 16) |
             (((uint32_t)((uint8_t *)p_encoded_data)[3]) << 24 ));
}


static bool all_req_attrs_parsed(ble_ancs_c_t * p_ancs)
{
    if (p_ancs->parse_info.expected_number_of_attrs == 0)
    {
        return true;
    }
    return false;
}

static bool attr_is_requested(ble_ancs_c_t * p_ancs, ble_ancs_c_attr_t attr)
{
    if (p_ancs->parse_info.p_attr_list[attr.attr_id].get == true)
    {
        return true;
    }
    return false;
}

/**@brief Function for parsing command id and notification id.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details UID and command ID will be received only once at the beginning of the first
 *          GATTC notification of a new attribute request for a given iOS notification.
 *
 * @param[in] p_ancs     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static ble_ancs_c_parse_state_t command_id_parse(ble_ancs_c_t  * p_ancs,
                                                 const uint8_t * p_data_src,
                                                 uint32_t      * index)
{
    ble_ancs_c_parse_state_t parse_state;

    p_ancs->parse_info.command_id = (ble_ancs_c_cmd_id_val_t) p_data_src[(*index)++];

    switch (p_ancs->parse_info.command_id)
    {
        case BLE_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES:
            p_ancs->evt.evt_type = BLE_ANCS_C_EVT_NOTIF_ATTRIBUTE;
            p_ancs->parse_info.p_attr_list  = p_ancs->ancs_notif_attr_list;
            p_ancs->parse_info.nb_of_attr   = BLE_ANCS_NB_OF_NOTIF_ATTR;
            parse_state                     = BLE_ANCS_NOTIF_UID;
            break;

        case BLE_ANCS_COMMAND_ID_GET_APP_ATTRIBUTES:
            p_ancs->evt.evt_type = BLE_ANCS_C_EVT_APP_ATTRIBUTE;
            p_ancs->parse_info.p_attr_list  = p_ancs->ancs_app_attr_list;
            p_ancs->parse_info.nb_of_attr   = BLE_ANCS_NB_OF_APP_ATTR;
            parse_state                     = BLE_ANCS_APP_ID;
            break;

        default:
            //no valid command_id, abort the rest of the parsing procedure.
            ESP_LOGD(TAG, "Invalid Command ID");
            parse_state = BLE_ANCS_ATTR_DONE;
            break;
    }
    return parse_state;
}


static ble_ancs_c_parse_state_t notif_uid_parse(ble_ancs_c_t  * p_ancs,
                                                const uint8_t * p_data_src,
                                                uint32_t      * index)
{
     p_ancs->evt.notif_uid = uint32_decode(&p_data_src[*index]);
     *index               += sizeof(uint32_t);
     return BLE_ANCS_ATTR_ID;
}

static ble_ancs_c_parse_state_t app_id_parse(ble_ancs_c_t  * p_ancs,
                                             const uint8_t * p_data_src,
                                             uint32_t      * index)
{
    p_ancs->evt.app_id[p_ancs->parse_info.current_app_id_index] = p_data_src[(*index)++];

    if (p_ancs->evt.app_id[p_ancs->parse_info.current_app_id_index] != '\0')
    {
        p_ancs->parse_info.current_app_id_index++;
        return BLE_ANCS_APP_ID;
    }
    else
    {
        return BLE_ANCS_ATTR_ID;
    }
}

/**@brief Function for parsing the id of an iOS attribute.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details We only request attributes that are registered with @ref ble_ancs_c_attr_add
 *          once they have been reveiced we stop parsing.
 *
 * @param[in] p_ancs     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static ble_ancs_c_parse_state_t attr_id_parse(ble_ancs_c_t  * p_ancs,
                                              const uint8_t * p_data_src,
                                              uint32_t      * index)
{
    p_ancs->evt.attr.attr_id     = p_data_src[(*index)++];

    if (p_ancs->evt.attr.attr_id >= p_ancs->parse_info.nb_of_attr)
    {
        ESP_LOGD(TAG, "Attribute ID Invalid.");
        return BLE_ANCS_ATTR_DONE;
    }
    p_ancs->evt.attr.p_attr_data = p_ancs->parse_info.p_attr_list[p_ancs->evt.attr.attr_id].p_attr_data;

    if (all_req_attrs_parsed(p_ancs))
    {
        ESP_LOGD(TAG, "All requested attributes received. ");
        return BLE_ANCS_ATTR_DONE;
    }
    else
    {
        if (attr_is_requested(p_ancs, p_ancs->evt.attr))
        {
            p_ancs->parse_info.expected_number_of_attrs--;
        }
        ESP_LOGD(TAG, "Attribute ID %"PRIu32" ", p_ancs->evt.attr.attr_id);
        return BLE_ANCS_ATTR_LEN1;
    }
}


/**@brief Function for parsing the length of an iOS attribute.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details The Length is 2 bytes. Since there is a chance we reveice the bytes in two different
 *          GATTC notifications, we parse only the first byte here and then set the state machine
 *          ready to parse the next byte.
 *
 * @param[in] p_ancs     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static ble_ancs_c_parse_state_t attr_len1_parse(ble_ancs_c_t * p_ancs, const uint8_t * p_data_src, uint32_t * index)
{
    p_ancs->evt.attr.attr_len = p_data_src[(*index)++];
    return BLE_ANCS_ATTR_LEN2;
}

/**@brief Function for parsing the length of an iOS attribute.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details Second byte of the length field. If the length is zero, it means that the attribute is not
 *          present and the state machine is set to parse the next attribute.
 *
 * @param[in] p_ancs     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static ble_ancs_c_parse_state_t attr_len2_parse(ble_ancs_c_t * p_ancs, const uint8_t * p_data_src, uint32_t * index)
{
    p_ancs->evt.attr.attr_len |= (p_data_src[(*index)++] << 8);
    p_ancs->parse_info.current_attr_index = 0;

    if (p_ancs->evt.attr.attr_len != 0)
    {
        //If the attribute has a length but there is no allocated space for this attribute
        if ((p_ancs->parse_info.p_attr_list[p_ancs->evt.attr.attr_id].attr_len == 0) ||
           (p_ancs->parse_info.p_attr_list[p_ancs->evt.attr.attr_id].p_attr_data == NULL))
        {
            return BLE_ANCS_ATTR_SKIP;
        }
        else
        {
            return BLE_ANCS_ATTR_DATA;
        }
    }
    else
    {

        ESP_LOGD(TAG, "Attribute LEN %u ", p_ancs->evt.attr.attr_len);
        if (attr_is_requested(p_ancs, p_ancs->evt.attr))
        {
            p_ancs->evt_handler(&p_ancs->evt, p_ancs->ctx);
        }
        if (all_req_attrs_parsed(p_ancs))
        {
            return BLE_ANCS_ATTR_DONE;
        }
        else
        {
            return BLE_ANCS_ATTR_ID;
        }
    }
}


/**@brief Function for parsing the data of an iOS attribute.
 *        Used in the @ref parse_get_notif_attrs_response state machine.
 *
 * @details Read the data of the attribute into our local buffer.
 *
 * @param[in] p_ancs     Pointer to an ANCS instance to which the event belongs.
 * @param[in] p_data_src Pointer to data that was received from the Notification Provider.
 * @param[in] index      Pointer to an index that helps us keep track of the current data to be parsed.
 *
 * @return The next parse state.
 */
static ble_ancs_c_parse_state_t attr_data_parse(ble_ancs_c_t  * p_ancs,
                                                const uint8_t * p_data_src,
                                                uint32_t      * index)
{
    // We have not reached the end of the attribute, nor our max allocated internal size.
    // Proceed with copying data over to our buffer.
    if (   (p_ancs->parse_info.current_attr_index < p_ancs->parse_info.p_attr_list[p_ancs->evt.attr.attr_id].attr_len)
        && (p_ancs->parse_info.current_attr_index < p_ancs->evt.attr.attr_len))
    {
        //ESP_LOGD(TAG, "Byte copied to buffer: %c", p_data_src[(*index)]); // Un-comment this line to see every byte of an attribute as it is parsed. Commented out by default since it can overflow the uart buffer.
        p_ancs->evt.attr.p_attr_data[p_ancs->parse_info.current_attr_index++] = p_data_src[(*index)++];
    }

    // We have reached the end of the attribute, or our max allocated internal size.
    // Stop copying data over to our buffer. NUL-terminate at the current index.
    if ( (p_ancs->parse_info.current_attr_index == p_ancs->evt.attr.attr_len) ||
         (p_ancs->parse_info.current_attr_index == p_ancs->parse_info.p_attr_list[p_ancs->evt.attr.attr_id].attr_len - 1))
    {
        if (attr_is_requested(p_ancs, p_ancs->evt.attr))
        {
            p_ancs->evt.attr.p_attr_data[p_ancs->parse_info.current_attr_index] = '\0';
        }

        // If our max buffer size is smaller than the remaining attribute data, we must
        // increase index to skip the data until the start of the next attribute.
        if (p_ancs->parse_info.current_attr_index < p_ancs->evt.attr.attr_len)
        {
            return BLE_ANCS_ATTR_SKIP;
        }
        ESP_LOGD(TAG, "Attribute finished!");
        if (attr_is_requested(p_ancs, p_ancs->evt.attr))
        {
            p_ancs->evt_handler(&p_ancs->evt, p_ancs->ctx);
        }
        if (all_req_attrs_parsed(p_ancs))
        {
            return BLE_ANCS_ATTR_DONE;
        }
        else
        {
            return BLE_ANCS_ATTR_ID;
        }
    }
    return BLE_ANCS_ATTR_DATA;
}


static ble_ancs_c_parse_state_t attr_skip(ble_ancs_c_t * p_ancs, const uint8_t * p_data_src, uint32_t * index)
{
    // We have not reached the end of the attribute, nor our max allocated internal size.
    // Proceed with copying data over to our buffer.
    if (p_ancs->parse_info.current_attr_index < p_ancs->evt.attr.attr_len)
    {
        p_ancs->parse_info.current_attr_index++;
        (*index)++;
    }
    // At the end of the attribute, determine if it should be passed to event handler and
    // continue parsing the next attribute ID if we are not done with all the attributes.
    if (p_ancs->parse_info.current_attr_index == p_ancs->evt.attr.attr_len)
    {
        if (attr_is_requested(p_ancs, p_ancs->evt.attr))
        {
            p_ancs->evt_handler(&p_ancs->evt, p_ancs->ctx);
        }
        if (all_req_attrs_parsed(p_ancs))
        {
            return BLE_ANCS_ATTR_DONE;
        }
        else
        {
            return BLE_ANCS_ATTR_ID;
        }
    }
    return BLE_ANCS_ATTR_SKIP;
}


/**@brief Function for checking whether the data in an iOS notification is out of bounds.
 *
 * @param[in] notif  An iOS notification.
 *
 * @retval ESP_OK              If the notification is within bounds.
 * @retval ESP_ERR_INVALID_ARG If the notification is out of bounds.
 */
static esp_err_t verify_notification_format(ble_ancs_c_evt_notif_t const * notif)
{
    if ((notif->evt_id >= BLE_ANCS_NB_OF_EVT_ID) || (notif->category_id >= BLE_ANCS_NB_OF_CATEGORY_ID))
    {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}


void ble_ancs_parse_get_attrs_response(ble_ancs_c_t  * p_ancs,
                                       const uint8_t * p_data_src,
                                       const uint16_t  hvx_data_len)
{
    uint32_t index;

    for (index = 0; index < hvx_data_len;)
    {
        switch (p_ancs->parse_info.parse_state)
        {
            case BLE_ANCS_COMMAND_ID:
                p_ancs->parse_info.parse_state = command_id_parse(p_ancs, p_data_src, &index);
                break;

            case BLE_ANCS_NOTIF_UID:
                p_ancs->parse_info.parse_state = notif_uid_parse(p_ancs, p_data_src, &index);
                break;

            case BLE_ANCS_APP_ID:
                p_ancs->parse_info.parse_state = app_id_parse(p_ancs, p_data_src, &index);
                break;

            case BLE_ANCS_ATTR_ID:
                p_ancs->parse_info.parse_state = attr_id_parse(p_ancs, p_data_src, &index);
                break;

            case BLE_ANCS_ATTR_LEN1:
                p_ancs->parse_info.parse_state = attr_len1_parse(p_ancs, p_data_src, &index);
                break;

            case BLE_ANCS_ATTR_LEN2:
                p_ancs->parse_info.parse_state = attr_len2_parse(p_ancs, p_data_src, &index);
                break;

            case BLE_ANCS_ATTR_DATA:
                p_ancs->parse_info.parse_state = attr_data_parse(p_ancs, p_data_src, &index);
                break;

            case BLE_ANCS_ATTR_SKIP:
                p_ancs->parse_info.parse_state = attr_skip(p_ancs, p_data_src, &index);
                break;

            case BLE_ANCS_ATTR_DONE:
                ESP_LOGD(TAG, "Parse state: Done ");
                index = hvx_data_len;
                break;

            default:
                // Default case will never trigger intentionally. Go to the ATTR_DONE state to minimize the consequences.
                p_ancs->parse_info.parse_state = BLE_ANCS_ATTR_DONE;
                break;
        }
    }
}

uint32_t ble_ancs_build_notif_attrs_request(ble_ancs_c_t * p_ancs,
                                            uint32_t const p_uid,
                                            uint8_t      * p_data,
                                            uint16_t const len)
{
    uint32_t index = 0;

    // Calculate buffer size needed for request
    index = sizeof(uint8_t) + sizeof(uint32_t); /*Command ID & Notification UID*/
    for (uint32_t attr = 0; attr < BLE_ANCS_NB_OF_NOTIF_ATTR; attr++)
    {
        if (p_ancs->ancs_notif_attr_list[attr].get == true)
        {
            index += sizeof(uint8_t); /*Attr*/

            if ((attr == BLE_ANCS_NOTIF_ATTR_ID_TITLE) ||
                (attr == BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE) ||
                (attr == BLE_ANCS_NOTIF_ATTR_ID_MESSAGE))
            {
                index += sizeof(uint16_t); /*Length*/
            }
        }
    }

    if (index > len) {
        return 0; // Buffer too small
    }

    index = 0;
    p_ancs->number_of_requested_attr = 0;

    //Encode Command ID.
    p_data[index++] = BLE_ANCS_COMMAND_ID_GET_NOTIF_ATTRIBUTES;

    //Encode Notification UID.
    index += uint32_encode(p_uid, &(p_data[index]));

    //Encode Attribute ID.
    for (uint32_t attr = 0; attr < BLE_ANCS_NB_OF_NOTIF_ATTR; attr++)
    {
        if (p_ancs->ancs_notif_attr_list[attr].get == true)
        {
            p_data[index++] = (uint8_t)attr;

            if ((attr == BLE_ANCS_NOTIF_ATTR_ID_TITLE) ||
                (attr == BLE_ANCS_NOTIF_ATTR_ID_SUBTITLE) ||
                (attr == BLE_ANCS_NOTIF_ATTR_ID_MESSAGE))
            {
                //Encode Length field. Only applicable for Title, Subtitle, and Message.
                index += uint16_encode(p_ancs->ancs_notif_attr_list[attr].attr_len,
                                       &(p_data[index]));
            }

            p_ancs->number_of_requested_attr++;
        }
    }

    p_ancs->parse_info.expected_number_of_attrs = p_ancs->number_of_requested_attr;

    return index;
}

//TODO: uint32_t ble_ancs_build_app_attrs_request()

esp_err_t ble_ancs_add_notif_attr(ble_ancs_c_t                       * p_ancs,
                                  ble_ancs_c_notif_attr_id_val_t const id,
                                  uint8_t                            * p_data,
                                  uint16_t const                      len)
{
    if (len == 0)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    p_ancs->ancs_notif_attr_list[id].get         = true;
    p_ancs->ancs_notif_attr_list[id].attr_len    = len;
    p_ancs->ancs_notif_attr_list[id].p_attr_data = p_data;

    return ESP_OK;
}

esp_err_t ble_ancs_add_app_attr(ble_ancs_c_t                     * p_ancs,
                                ble_ancs_c_app_attr_id_val_t const id,
                                uint8_t                          * p_data,
                                uint16_t const                     len)
{
    if (len == 0)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    p_ancs->ancs_app_attr_list[id].get         = true;
    p_ancs->ancs_app_attr_list[id].attr_len    = len;
    p_ancs->ancs_app_attr_list[id].p_attr_data = p_data;

    return ESP_OK;
}

esp_err_t ble_ancs_parse_notif(ble_ancs_c_t const * p_ancs,
                               uint8_t      const * p_data_src,
                               uint16_t     const   hvx_data_len)
{
    ble_ancs_c_evt_t ancs_evt;
    uint32_t         err_code;
    if (hvx_data_len != BLE_ANCS_NOTIFICATION_DATA_LENGTH)
    {
        ancs_evt.evt_type = BLE_ANCS_C_EVT_INVALID_NOTIF;
        p_ancs->evt_handler(&ancs_evt, p_ancs->ctx);
        return ESP_ERR_INVALID_ARG;
    }

    #define BLE_ANCS_NOTIF_EVT_ID_INDEX       0   /**< Index of the Event ID field when parsing notifications. */
    #define BLE_ANCS_NOTIF_FLAGS_INDEX        1   /**< Index of the Flags field when parsing notifications. */
    #define BLE_ANCS_NOTIF_CATEGORY_ID_INDEX  2   /**< Index of the Category ID field when parsing notifications. */
    #define BLE_ANCS_NOTIF_CATEGORY_CNT_INDEX 3   /**< Index of the Category Count field when parsing notifications. */
    #define BLE_ANCS_NOTIF_NOTIF_UID          4   /**< Index of the Notification UID field when parsing notifications. */

    ancs_evt.notif.evt_id = (ble_ancs_c_evt_id_values_t) p_data_src[BLE_ANCS_NOTIF_EVT_ID_INDEX];
    ancs_evt.notif.evt_flags.silent = (p_data_src[BLE_ANCS_NOTIF_FLAGS_INDEX] >> BLE_ANCS_EVENT_FLAG_SILENT) & 0x01;
    ancs_evt.notif.evt_flags.important = (p_data_src[BLE_ANCS_NOTIF_FLAGS_INDEX] >> BLE_ANCS_EVENT_FLAG_IMPORTANT) & 0x01;
    ancs_evt.notif.evt_flags.pre_existing = (p_data_src[BLE_ANCS_NOTIF_FLAGS_INDEX] >> BLE_ANCS_EVENT_FLAG_PREEXISTING) & 0x01;
    ancs_evt.notif.evt_flags.positive_action = (p_data_src[BLE_ANCS_NOTIF_FLAGS_INDEX] >> BLE_ANCS_EVENT_FLAG_POSITIVE_ACTION) & 0x01;
    ancs_evt.notif.evt_flags.negative_action = (p_data_src[BLE_ANCS_NOTIF_FLAGS_INDEX] >> BLE_ANCS_EVENT_FLAG_NEGATIVE_ACTION) & 0x01;
    ancs_evt.notif.category_id = (ble_ancs_c_category_id_val_t) p_data_src[BLE_ANCS_NOTIF_CATEGORY_ID_INDEX];
    ancs_evt.notif.category_count = p_data_src[BLE_ANCS_NOTIF_CATEGORY_CNT_INDEX];
    ancs_evt.notif.notif_uid = uint32_decode(&p_data_src[BLE_ANCS_NOTIF_NOTIF_UID]);

    err_code = verify_notification_format(&ancs_evt.notif);
    if (err_code == ESP_OK)
    {
        ancs_evt.evt_type = BLE_ANCS_C_EVT_NOTIF;
    }
    else
    {
        ancs_evt.evt_type = BLE_ANCS_C_EVT_INVALID_NOTIF;
    }

    p_ancs->evt_handler(&ancs_evt, p_ancs->ctx);
    return err_code;
}
