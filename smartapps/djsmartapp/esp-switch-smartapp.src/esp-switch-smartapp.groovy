/**
 *  ESP Switch SmartApp
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
definition(
		name: "ESP Switch SmartApp",
		namespace: "djsmartapp",
		author: "David Jung",
		description: "Support switch devices using ESP8266. No frills",
		category: "Convenience",
		iconUrl: "https://s3.amazonaws.com/smartapp-icons/Convenience/Cat-Convenience.png",
		iconX2Url: "https://s3.amazonaws.com/smartapp-icons/Convenience/Cat-Convenience@2x.png",
		iconX3Url: "https://s3.amazonaws.com/smartapp-icons/Convenience/Cat-Convenience@2x.png")


preferences {
	page(name: "deviceDiscovery", title: "ESP Switch Setup", content: "deviceDiscovery")
}

mappings{
	path("/update/:id/:status"){
    	action: [GET: "updateStatus"]
	}
    path("/switches"){
    	action: [GET: "list"]
	}
}

def deviceDiscovery() {
	cleanDeviceList()
	def options = [:]
	def devices = getVerifiedDevices()
	devices.each {
		def value = it.value.name ?: "UPnP Device ${it.value.ssdpUSN.split(':')[1][-3..-1]}"
		def key = it.value.mac
		options["${key}"] = value
	}

	ssdpSubscribe()

	ssdpDiscover()
	verifyDevices()

	return dynamicPage(name: "deviceDiscovery", title: "Discovery Started!", nextPage: "", refreshInterval: 5, install: true, uninstall: true) {
		section("Please wait while your ESP Switches are discovered. Discovery can take five minutes or more.") {
			input "selectedDevices", "enum", required: false, title: "Select Devices (${options.size() ?: 0} devices discovered)", multiple: true, options: options
		}
	}
}

def installed() {
	initialize()
}

def updated() {
	unsubscribe()
	initialize()
}

def initialize() {
	unsubscribe()
	unschedule()

	ssdpSubscribe()

	if (selectedDevices) {
		addDevices()
	}

	runEvery5Minutes("ssdpDiscover")
}

void ssdpDiscover() {
	sendHubCommand(new physicalgraph.device.HubAction("lan discovery urn:schemas-upnp-org:device:Basic:1", physicalgraph.device.Protocol.LAN))
}

void ssdpSubscribe() {
	subscribe(location, "ssdpTerm.urn:schemas-upnp-org:device:Basic:1", ssdpHandler)
}

Map verifiedDevices() {
	def devices = getVerifiedDevices()
	def map = [:]
	devices.each {
		def value = it.value.name ?: "UPnP Device ${it.value.ssdpUSN.split(':')[1][-3..-1]}"
		def key = it.value.mac
		map["${key}"] = value
	}
	map
}

void verifyDevices() {
	def devices = getDevices().findAll { it?.value?.verified != true }
	devices.each {
		int port = convertHexToInt(it.value.deviceAddress)
		String ip = convertHexToIP(it.value.networkAddress)
		String host = "${ip}:${port}"
		sendHubCommand(new physicalgraph.device.HubAction("""GET ${it.value.ssdpPath} HTTP/1.1\r\nHOST: $host\r\n\r\n""", physicalgraph.device.Protocol.LAN, host, [callback: deviceDescriptionHandler]))
	}
}

def getVerifiedDevices() {
	getDevices().findAll{ it.value.verified == true }
}

def getDevices() {
	if (!state.devices) {
		state.devices = [:]
	}
	state.devices
}

def addDevices() {
	def devices = getDevices()

	selectedDevices.each { dni ->
		def selectedDevice = devices.find { it.value.mac == dni }
		def d
		if (selectedDevice) {
			d = getChildDevices()?.find {
				it.deviceNetworkId == selectedDevice.value.mac
			}
		}

		if (!d) {
			def current = addChildDevice("djdevice", "ESP Switch", selectedDevice.value.mac, selectedDevice?.value.hub, [
				"label": selectedDevice?.value?.name ?: "ESP Switch",
				"data": [
					"mac": selectedDevice.value.mac,
					"ip": selectedDevice.value.networkAddress,
					"port": selectedDevice.value.deviceAddress
				]
			])
			sendCallback(current, selectedDevice.value.networkAddress, selectedDevice.value.deviceAddress)
		}
	}
}

def cleanDeviceList()
{
	// Delete all the devices stored in the smart app device list. This device list contains every single device that smart app detected, whether it is usable by this app or not.
    // This list conatins deleted devices and provide them as potentially usable devices.
	def devices = getDevices()
    def children = getChildDevices()
    def toBeDeleted = []
    devices.each { d ->
    	def target = children.find {it == d.value.mac}
        if(target == null){
        	toBeDeleted.add(d.key)
        }
    }
    
    toBeDeleted.each {d ->
    	state.devices.remove(d)
    }
}

def ssdpHandler(evt) {
	def description = evt.description
	def hub = evt?.hubId

	def parsedEvent = parseLanMessage(description)
	parsedEvent << ["hub":hub]

	def devices = getDevices()
	String ssdpUSN = parsedEvent.ssdpUSN.toString()
	if (devices."${ssdpUSN}") {
		def d = devices."${ssdpUSN}"
        def child = getChildDevice(parsedEvent.mac)
		if (d.networkAddress != parsedEvent.networkAddress || d.deviceAddress != parsedEvent.deviceAddress) {
			d.networkAddress = parsedEvent.networkAddress
			d.deviceAddress = parsedEvent.deviceAddress
			if (child) {
				child.sync(parsedEvent.networkAddress, parsedEvent.deviceAddress)
			}
		}
        if(child)
        {
        	sendCallback(child, parsedEvent.networkAddress, parsedEvent.deviceAddress)
        }
	} else {
		devices << ["${ssdpUSN}": parsedEvent]
	}
}

void deviceDescriptionHandler(physicalgraph.device.HubResponse hubResponse) {
	def body = hubResponse.xml
	def devices = getDevices()
    def model = body?.device?.modelName?.text()
    if(model == "SmartThings Compatible ESP Switch"){
        def device = devices.find { it?.key?.contains(body?.device?.UDN?.text()) }
        if (device) {
            device.value << [name: body?.device?.friendlyName?.text(), model:body?.device?.modelName?.text(), serialNumber:body?.device?.serialNum?.text(), verified: true]
        }
    }
}

void sendCallback(device, ip, port)
{
    if(!state.accessToken)
    {
    	createAccessToken()
    }
   	
    def host = ip + ":" + port
    
    try {
    	def appId = getApp().getId()
    	def url = apiServerUrl("api/smartapps/installations/" + appId + "/update/" + device.getId())
		def hubAction = new physicalgraph.device.HubAction(
			method: "GET",
			path: "/callback?url=${url}&token=${state.accessToken}",
            headers: [
            	HOST: "${host}"
            ])
        
        sendHubCommand(hubAction)
	}
	catch (Exception e) {
		log.debug "Hit Exception $e on $hubAction"
	}
}

// Update logic
void updateStatus(){
	def status = params.status
    if(status)
    {
    	def devices = getChildDevices()
        def device = devices.find {it.getId() == params.id}
        
        if(device)
        {
        	device."status${status}"()
        }
    }
}

// SmartThings helpers
private Integer convertHexToInt(hex) {
	Integer.parseInt(hex,16)
}

private String convertHexToIP(hex) {
	[convertHexToInt(hex[0..1]),convertHexToInt(hex[2..3]),convertHexToInt(hex[4..5]),convertHexToInt(hex[6..7])].join(".")
}