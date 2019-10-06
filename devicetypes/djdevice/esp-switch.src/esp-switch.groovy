/**
 *  ESP Switch Device
 *
 *  Copyright 2019 David Jung
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 *  in compliance with the License. You may obtain a copy of the License at:
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software distributed under the License is distributed
 *  on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License
 *  for the specific language governing permissions and limitations under the License.
 *
 */
metadata {
	definition (name: "ESP Switch", namespace: "djdevice", author: "David Jung") {
		capability "Switch"
		capability "Relay Switch"
		capability "Actuator"
		capability "Sensor"
	}

	simulator {

	}

	tiles(scale: 2) {
		multiAttributeTile(name:"switch", type: "lighting", width: 3, height: 4, canChangeIcon: true){
			tileAttribute ("device.switch", key: "PRIMARY_CONTROL") {
				attributeState "off", label: '${name}', action: "switch.on", icon: "st.switches.switch.off", backgroundColor: "#ffffff", nextState:"turningOn"
				attributeState "on", label: '${name}', action: "switch.off", icon: "st.switches.switch.on", backgroundColor: "#00A0DC", nextState:"turningOff"
				attributeState "turningOn", label:'${name}', action:"switch.off", icon:"st.switches.switch.on", backgroundColor:"#00A0DC", nextState:"turningOff"
				attributeState "turningOff", label:'${name}', action:"switch.on", icon:"st.switches.switch.off", backgroundColor:"#ffffff", nextState:"turningOn"
			}
		}
	}
}

// parse events into attributes
def parse(String description) {
	try{
    	def msg = parseLanMessage(description)
        def body = msg.body
        if(body == "On")
        	return createEvent(name: "switch", value: "on",isStateChange : true)
        else if(body =="Off")
        	return createEvent(name: "switch", value: "off",isStateChange : true)
    }
    catch (Exception e) {
		log.debug "Hit Exception $e on parsing"
	}
}

// If the switch gets different IP address, it will update through this method.
def sync(ip, port) {
	def existingIp = getDataValue("ip")
	def existingPort = getDataValue("port")
	if (ip && ip != existingIp) {
		updateDataValue("ip", ip)
	}
	if (port && port != existingPort) {
		updateDataValue("port", port)
	}
}

// These are called when the status has been changed outside of SmartThings infrastructure. Updates were pushed through SmartApp's API
def statusOn(){
	sendEvent(name: "switch", value: "on",isStateChange : true)
}
def statusOff(){
	sendEvent(name: "switch", value: "off",isStateChange : true)
}

// Switch interface implementations
def on()
{
	action("on")
}

def off()
{
	action("off")
}

def action(value)
{
   	def ip = getDataValue("ip")
	def port = getDataValue("port")
    def host = ip + ":" + port
	try {
		def hubAction = new physicalgraph.device.HubAction(
			method: "GET",
			path: "/${value}",
            headers: [
            	HOST: "${host}"
            ])
        
        sendHubCommand(hubAction)
	}
	catch (Exception e) {
		log.debug "Hit Exception $e on $hubAction"
	}
}

