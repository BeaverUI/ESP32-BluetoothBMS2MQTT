
// connect and subscribe
int MQTTconnect(void) {
  // Retry until connected
  static unsigned long prev_millis=0;

  while(!mqttclient.connected()){
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    int retVal;
    if(strcmp(MQTT_USERNAME, "") == 0){
      retVal=mqttclient.connect(NODE_NAME);
    }else{
      retVal=mqttclient.connect(NODE_NAME,MQTT_USERNAME,MQTT_PASSWORD);
    }
  
    if (retVal) {
      Serial.println("connected");
      return retVal;
    } else {
      Serial.print("failed");
    }
  }

  return true;
}

void MqttDebug(const char* msg){
  MqttDebug(String(msg));
}

void MqttDebug(String msg){
  debug_log_string += msg + "(|)";
}

void MqttPublishDebug(){
  if(debug_log_string!=""){
    // spit out all debug messages that were stored
    int index_start=0;
    int index_end=0;
    unsigned int k=0;

    while(true){
      index_end=debug_log_string.indexOf(String("(|)"), index_start+1);
      if((index_end<1)||(k++>20)){ // max 20 messages
        break;
      }
               
      mqttclient.publish(GetTopic("debug"), debug_log_string.substring(index_start,index_end)); 
      
      index_start=index_end+3;
    }
    debug_log_string="";  // clear log string
  }  
}


String GetTopic(String topic){
  return String(NODE_NAME) + String("/") + String(topic);
}
