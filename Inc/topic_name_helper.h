#include<string.h>

#define LEDS_TOPIC 1
#define LEDH_TOPIC 2
#define LIGHT_TOPIC 3
#define MODE_TOPIC 4

const char* leds_topic_name = "leds";
const char* ledh_topic_name = "ledh";
const char* light_topic_name = "light";
const char* mode_topic_name = "mode";

int getTopicCode(const char* topic_name){
    if(!strcmp(topic_name,leds_topic_name))
        return LEDS_TOPIC;
    if(!strcmp(topic_name,ledh_topic_name))
        return LEDH_TOPIC;
    if(!strcmp(topic_name,light_topic_name))
        return LIGHT_TOPIC;
    if(!strcmp(topic_name,mode_topic_name))
        return MODE_TOPIC;    
}

// Only parse the first one char to unsigned int.
int getPayLoadValue(const char* payload){
    return payload[0]-48;
}
