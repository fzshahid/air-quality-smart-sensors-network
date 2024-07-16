// Define a larger buffer size
#define SENSOR_DATA_BUFFER_SIZE 512

static void coap_send_data_request(struct k_work *work)
{
    char sensors_data[SENSOR_DATA_BUFFER_SIZE];
    otError error = OT_ERROR_NONE;
    otMessage *myMessage;
    otMessageInfo myMessageInfo;
    otInstance *myInstance = openthread_get_default_instance();
    const otMeshLocalPrefix *ml_prefix = otThreadGetMeshLocalPrefix(myInstance);
    uint8_t serverInterfaceID[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    const char *serverIpAddr = "fd00:0:fb01:1:c9bd:dc9d:23e:82c5";
    const char *myTemperatureJson = "{\"temperature\": 23.32}";

    printk("Sending payload!\n");

    do
    {
        read_scd41();
        read_ccs811(ccs811);
        read_sps30();

        // Create the sensor data JSON string
        int ret = snprintf(sensors_data, sizeof(sensors_data),
                           "{\"CO2\": %.2f, \"Humidity\": %.2f, \"Temperature\": %.2f, \"eCO2\": %.2f, \"TVOC\": %.2f, "
                           "\"PM1.0\": %.2f, \"PM2.5\": %.2f, \"PM4.0\": %.2f, \"PM10\": %.2f}",
                           sensor_value_to_double(&co2),
                           sensor_value_to_double(&humidity) / 1000.0,
                           sensor_value_to_double(&temperature) / 1000.0,
                           sensor_value_to_double(&eco2),
                           sensor_value_to_double(&tvoc),
                           m.mc_1p0, m.mc_2p5, m.mc_4p0, m.mc_10p0);

        if (ret < 0 || ret >= sizeof(sensors_data))
        {
            printk("Error creating sensor data string, snprintf returned %d\n", ret);
            break;
        }

        myMessage = otCoapNewMessage(myInstance, NULL);
        if (myMessage == NULL)
        {
            printk("Failed to allocate message for CoAP Request\n");
            return;
        }
        otCoapMessageInit(myMessage, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_PUT);
        error = otCoapMessageAppendUriPathOptions(myMessage, "storedata");
        if (error != OT_ERROR_NONE)
        {
            break;
        }
        error = otCoapMessageAppendContentFormatOption(myMessage,
                                                       OT_COAP_OPTION_CONTENT_FORMAT_JSON);
        if (error != OT_ERROR_NONE)
        {
            break;
        }
        error = otCoapMessageSetPayloadMarker(myMessage);
        if (error != OT_ERROR_NONE)
        {
            break;
        }

        printk("%s\n", sensors_data);

        error = otMessageAppend(myMessage, sensors_data,
                                strlen(sensors_data));
        if (error != OT_ERROR_NONE)
        {
            break;
        }
        memset(&myMessageInfo, 0, sizeof(myMessageInfo));
        memset(sensors_data, 0, sizeof(sensors_data));
        myMessageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;

        error = otIp6AddressFromString(serverIpAddr, &myMessageInfo.mPeerAddr);
        if (error != OT_ERROR_NONE)
        {
            break;
        }

        error = otCoapSendRequest(myInstance, myMessage, &myMessageInfo,
                                  coap_send_data_response_cb, NULL);

    } while (false);
    if (error != OT_ERROR_NONE)
    {
        printk("Failed to send CoAP Request: %d\n", error);
        otMessageFree(myMessage);
    }
    else
    {
        printk("CoAP data sent.\n");
    }
}
