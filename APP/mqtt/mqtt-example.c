/*
 * Copyright (c) 2014-2016 Alibaba Group. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mico.h"

#include "iot_export.h"
#include "iot_import.h"
#include "lite-log.h"

#if defined(MQTT_ID2_AUTH) && defined(TEST_ID2_DAILY)
    #define PRODUCT_KEY             "*******************"
    #define DEVICE_NAME             "*******************"
    #define DEVICE_SECRET           "*******************"
#else
    #define PRODUCT_KEY             "PvvDkPSUA9m"
//    #define DEVICE_NAME             "device1"
//    #define DEVICE_SECRET           "7rxJKOvVYBrMgA0MnNuFKVBTUtSWHxxr"
    #define DEVICE_NAME             "device2"
    #define DEVICE_SECRET           "YtZrcoZEVanPyy6DyUdJyXJ4ZzL4smfU"
#endif

// These are pre-defined topics
#define TOPIC_UPDATE            "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
#define TOPIC_ERROR             "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
#define TOPIC_GET               "/"PRODUCT_KEY"/"DEVICE_NAME"/get"
#define TOPIC_DATA              "/"PRODUCT_KEY"/"DEVICE_NAME"/data"

#define MSG_LEN_MAX             (1024)


#define aliot_mqtt_log(format, ...)  custom_log("aliot_mqtt1", format, ##__VA_ARGS__)



static int      user_argc;
static char   **user_argv;

void event_handle(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type) {
        case IOTX_MQTT_EVENT_UNDEF:
            aliot_mqtt_log("undefined event occur.");
            break;

        case IOTX_MQTT_EVENT_DISCONNECT:
            aliot_mqtt_log("MQTT disconnect.");
            break;

        case IOTX_MQTT_EVENT_RECONNECT:
            aliot_mqtt_log("MQTT reconnect.");
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_SUCCESS:
            aliot_mqtt_log("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_TIMEOUT:
            aliot_mqtt_log("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_NACK:
            aliot_mqtt_log("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
            aliot_mqtt_log("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
            aliot_mqtt_log("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_NACK:
            aliot_mqtt_log("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_SUCCESS:
            aliot_mqtt_log("publish success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_TIMEOUT:
            aliot_mqtt_log("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_NACK:
            aliot_mqtt_log("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_RECVEIVED:
            aliot_mqtt_log("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                          topic_info->topic_len,
                          topic_info->ptopic,
                          topic_info->payload_len,
                          topic_info->payload);
            break;

        default:
            aliot_mqtt_log("Should NOT arrive here.");
            break;
    }
}

static void _demo_message_arrive(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    iotx_mqtt_topic_info_pt ptopic_info = (iotx_mqtt_topic_info_pt) msg->msg;

    // print topic name and topic message
    aliot_mqtt_log("----");
    aliot_mqtt_log("Topic: '%.*s' (Length: %d)",
                  ptopic_info->topic_len,
                  ptopic_info->ptopic,
                  ptopic_info->topic_len);
    aliot_mqtt_log("Payload: '%.*s' (Length: %d)",
                  ptopic_info->payload_len,
                  ptopic_info->payload,
                  ptopic_info->payload_len);
    aliot_mqtt_log("----");
}

int mqtt_client(void)
{
    int rc = 0, msg_len, cnt = 0;
    void *pclient;
    iotx_conn_info_pt pconn_info;
    iotx_mqtt_param_t mqtt_params;
    iotx_mqtt_topic_info_t topic_msg;
    char msg_pub[128];
    char *msg_buf = NULL, *msg_readbuf = NULL;

    if (NULL == (msg_buf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        aliot_mqtt_log("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if (NULL == (msg_readbuf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        aliot_mqtt_log("not enough memory");
        rc = -1;
        goto do_exit;
    }
    /* Init gprs hardware */
    lite_phy_net_connect();
    /* Device AUTH */
    if (0 != IOT_SetupConnInfo(PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, (void **)&pconn_info)) {
        aliot_mqtt_log("AUTH request failed!");
        rc = -1;
        goto do_exit;
    }
    aliot_mqtt_log("after auth ............ memory");

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 2000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.pread_buf = msg_readbuf;
    mqtt_params.read_buf_size = MSG_LEN_MAX;
    mqtt_params.pwrite_buf = msg_buf;
    mqtt_params.write_buf_size = MSG_LEN_MAX;

    mqtt_params.handle_event.h_fp = event_handle;
    mqtt_params.handle_event.pcontext = NULL;

    /* Construct a MQTT client with specify parameter */
    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        aliot_mqtt_log("MQTT construct failed");
        rc = -1;
        goto do_exit;
    }

    /* Subscribe the specific topic */
    rc = IOT_MQTT_Subscribe(pclient, TOPIC_DATA, IOTX_MQTT_QOS1, _demo_message_arrive, NULL); //recv 90 03 00 01 01
    if (rc < 0) {
        IOT_MQTT_Destroy(&pclient);
        aliot_mqtt_log("IOT_MQTT_Subscribe() failed, rc = %d", rc);
        rc = -1;
        goto do_exit;
    }

    HAL_SleepMs(1000);

    /* Initialize topic information */
    memset(&topic_msg, 0x0, sizeof(iotx_mqtt_topic_info_t));
    strcpy(msg_pub, "message: hello! start!");

    topic_msg.qos = IOTX_MQTT_QOS1;
    topic_msg.retain = 0;
    topic_msg.dup = 0;
    topic_msg.payload = (void *)msg_pub;
    topic_msg.payload_len = strlen(msg_pub);

    rc = IOT_MQTT_Publish(pclient, TOPIC_DATA, &topic_msg);  //recv 40 02 00 02
    aliot_mqtt_log("rc = IOT_MQTT_Publish() = %d", rc);    //recv 35 33 2C 32 33 00 19 2F 50

    do {
        /* Generate topic message */
        cnt++;
        msg_len = snprintf(msg_pub, sizeof(msg_pub), "{\"attr_name\":\"temperature\", \"attr_value\":\"%d\"}", cnt);
        if (msg_len < 0) {
            aliot_mqtt_log("Error occur! Exit program");
            rc = -1;
            break;
        }

        topic_msg.payload = (void *)msg_pub;
        topic_msg.payload_len = msg_len;

        rc = IOT_MQTT_Publish(pclient, TOPIC_DATA, &topic_msg);// recv 40 02 00 03
        if (rc < 0) {										   // recv 37 36 2C 32 4A 00 19 2F 50
            aliot_mqtt_log("error occur when publish");
            rc = -1;
            break;
        }
#ifdef MQTT_ID2_CRYPTO
        aliot_mqtt_log("packet-id=%u, publish topic msg='0x%02x%02x%02x%02x'...",
                      (uint32_t)rc,
                      msg_pub[0], msg_pub[1], msg_pub[2], msg_pub[3]
                     );
#else
        aliot_mqtt_log("packet-id=%u, publish topic msg=%s", (uint32_t)rc, msg_pub);
#endif

        /* handle the MQTT packet received from TCP or SSL connection */
        IOT_MQTT_Yield(pclient, 2000);

        /* infinite loop if running with 'loop' argument */
        if (user_argc >= 2 && !strcmp("loop", user_argv[1])) {
            HAL_SleepMs(2000);
            cnt = 0;
        }

    } while (cnt < 1);

    IOT_MQTT_Unsubscribe(pclient, TOPIC_DATA);

    HAL_SleepMs(200);

    IOT_MQTT_Destroy(&pclient);

do_exit:
    if (NULL != msg_buf) {
        HAL_Free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        HAL_Free(msg_readbuf);
    }

    return rc;
}

void mqtt_client_thread( mico_thread_arg_t arg )
{
    UNUSED_PARAMETER( arg );
    mqtt_client();
    mico_rtos_delete_thread( NULL );
}

int application_start(void)
{
	OSStatus err;
    IOT_OpenLog("mqtt");
    IOT_SetLogLevel(IOT_LOG_DEBUG);

    aliot_mqtt_log("start test demo.");
    mqtt_client();

    IOT_DumpMemoryStats(IOT_LOG_DEBUG);
    IOT_CloseLog();

    aliot_mqtt_log("out of sample!");
exit:
	mico_rtos_delete_thread( NULL );
    return 0;
}

